/*
 * Copyright (C) 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "misc.h"
#include "Parcel.h"
#include "String8.h"
#include "String16.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef INT32_MAX
#define INT32_MAX ((int32_t)(2147483647))
#endif

// ---------------------------------------------------------------------------

#define PAD_SIZE(s) (((s)+3)&~3)

// XXX This can be made public if we want to provide
// support for typed data.
struct small_flat_data
{
    uint32_t type;
    uint32_t data;
};

namespace android {

// ---------------------------------------------------------------------------

Parcel::Parcel()
{
    initState();
}

Parcel::~Parcel()
{
    freeDataNoInit();
}

const uint8_t* Parcel::data() const
{
    return mData;
}

size_t Parcel::dataSize() const
{
    return (mDataSize > mDataPos ? mDataSize : mDataPos);
}

size_t Parcel::dataAvail() const
{
    // TODO: decide what to do about the possibility that this can
    // report an available-data size that exceeds a Java int's max
    // positive value, causing havoc.  Fortunately this will only
    // happen if someone constructs a Parcel containing more than two
    // gigabytes of data, which on typical phone hardware is simply
    // not possible.
    return dataSize() - dataPosition();
}

size_t Parcel::dataPosition() const
{
    return mDataPos;
}

size_t Parcel::dataCapacity() const
{
    return mDataCapacity;
}

status_t Parcel::setDataSize(size_t size)
{
    status_t err;
    err = continueWrite(size);
    if (err == NO_ERROR) {
        mDataSize = size;
    }
    return err;
}

void Parcel::setDataPosition(size_t pos) const
{
    mDataPos = pos;
    mNextObjectHint = 0;
}

status_t Parcel::setDataCapacity(size_t size)
{
    if (size > mDataSize) return continueWrite(size);
    return NO_ERROR;
}

status_t Parcel::setData(const uint8_t* buffer, size_t len)
{
    status_t err = restartWrite(len);
    if (err == NO_ERROR) {
        memcpy(const_cast<uint8_t*>(data()), buffer, len);
        mDataSize = len;
        mFdsKnown = false;
    }
    return err;
}

status_t Parcel::writeInterfaceToken(const String16& interface)
{
    // currently the interface identification token is just its name as a string
    return writeString16(interface);
}

bool Parcel::enforceInterface(const String16& interface) const
{
    String16 str = readString16();
    if (str == interface) {
        return true;
    } else {
        return false;
    }
} 

const size_t* Parcel::objects() const
{
    return mObjects;
}

size_t Parcel::objectsCount() const
{
    return mObjectsSize;
}

status_t Parcel::errorCheck() const
{
    return mError;
}

void Parcel::setError(status_t err)
{
    mError = err;
}

status_t Parcel::finishWrite(size_t len)
{
    //printf("Finish write of %d\n", len);
    mDataPos += len;
    if (mDataPos > mDataSize) {
        mDataSize = mDataPos;
    }
    //printf("New pos=%d, size=%d\n", mDataPos, mDataSize);
    return NO_ERROR;
}

status_t Parcel::writeUnpadded(const void* data, size_t len)
{
    size_t end = mDataPos + len;
    if (end < mDataPos) {
        // integer overflow
        return BAD_VALUE;
    }

    if (end <= mDataCapacity) {
restart_write:
        memcpy(mData+mDataPos, data, len);
        return finishWrite(len);
    }

    status_t err = growData(len);
    if (err == NO_ERROR) goto restart_write;
    return err;
}

status_t Parcel::write(const void* data, size_t len)
{
    void* const d = writeInplace(len);
    if (d) {
        memcpy(d, data, len);
        return NO_ERROR;
    }
    return mError;
}

void* Parcel::writeInplace(size_t len)
{
    const size_t padded = PAD_SIZE(len);

    // sanity check for integer overflow
    if (mDataPos+padded < mDataPos) {
        return NULL;
    }

    if ((mDataPos+padded) <= mDataCapacity) {
restart_write:
        //printf("Writing %ld bytes, padded to %ld\n", len, padded);
        uint8_t* const data = mData+mDataPos;

        // Need to pad at end?
        if (padded != len) {
#if BYTE_ORDER == BIG_ENDIAN
            static const uint32_t mask[4] = {
                0x00000000, 0xffffff00, 0xffff0000, 0xff000000
            };
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
            static const uint32_t mask[4] = {
                0x00000000, 0x00ffffff, 0x0000ffff, 0x000000ff
            };
#endif
            //printf("Applying pad mask: %p to %p\n", (void*)mask[padded-len],
            //    *reinterpret_cast<void**>(data+padded-4));
            *reinterpret_cast<uint32_t*>(data+padded-4) &= mask[padded-len];
        }

        finishWrite(padded);
        return data;
    }

    status_t err = growData(padded);
    if (err == NO_ERROR) goto restart_write;
    return NULL;
}

status_t Parcel::writeInt32(int32_t val)
{
    if ((mDataPos+sizeof(val)) <= mDataCapacity) {
restart_write:
        *reinterpret_cast<int32_t*>(mData+mDataPos) = val;
        return finishWrite(sizeof(val));
    }

    status_t err = growData(sizeof(val));
    if (err == NO_ERROR) goto restart_write;
    return err;
}

status_t Parcel::writeInt64(int64_t val)
{
    if ((mDataPos+sizeof(val)) <= mDataCapacity) {
restart_write:
        *reinterpret_cast<int64_t*>(mData+mDataPos) = val;
        return finishWrite(sizeof(val));
    }

    status_t err = growData(sizeof(val));
    if (err == NO_ERROR) goto restart_write;
    return err;
}

status_t Parcel::writeFloat(float val)
{
    if ((mDataPos+sizeof(val)) <= mDataCapacity) {
restart_write:
        *reinterpret_cast<float*>(mData+mDataPos) = val;
        return finishWrite(sizeof(val));
    }

    status_t err = growData(sizeof(val));
    if (err == NO_ERROR) goto restart_write;
    return err;
}

status_t Parcel::writeDouble(double val)
{
    if ((mDataPos+sizeof(val)) <= mDataCapacity) {
restart_write:
        *reinterpret_cast<double*>(mData+mDataPos) = val;
        return finishWrite(sizeof(val));
    }

    status_t err = growData(sizeof(val));
    if (err == NO_ERROR) goto restart_write;
    return err;
}

status_t Parcel::writeCString(const char* str)
{
    return write(str, strlen(str)+1);
}

status_t Parcel::writeString8(const String8& str)
{
    status_t err = writeInt32(str.bytes());
    if (err == NO_ERROR) {
        err = write(str.string(), str.bytes()+1);
    }
    return err;
}

status_t Parcel::writeString16(const String16& str)
{
    return writeString16(str.string(), str.size());
}

status_t Parcel::writeString16(const char16_t* str, size_t len)
{
    if (str == NULL) return writeInt32(-1);
    
    status_t err = writeInt32(len);
    if (err == NO_ERROR) {
        len *= sizeof(char16_t);
        uint8_t* data = (uint8_t*)writeInplace(len+sizeof(char16_t));
        if (data) {
            memcpy(data, str, len);
            *reinterpret_cast<char16_t*>(data+len) = 0;
            return NO_ERROR;
        }
        err = mError;
    }
    return err;
}

void Parcel::remove(size_t start, size_t amt)
{
    fprintf(stderr, "Parcel::remove() not yet implemented!");
    exit(1);
}

status_t Parcel::read(void* outData, size_t len) const
{
    if ((mDataPos+PAD_SIZE(len)) >= mDataPos && (mDataPos+PAD_SIZE(len)) <= mDataSize) {
        memcpy(outData, mData+mDataPos, len);
        mDataPos += PAD_SIZE(len);
        return NO_ERROR;
    }
    return NOT_ENOUGH_DATA;
}

const void* Parcel::readInplace(size_t len) const
{
    if ((mDataPos+PAD_SIZE(len)) >= mDataPos && (mDataPos+PAD_SIZE(len)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += PAD_SIZE(len);
        return data;
    }
    return NULL;
}

status_t Parcel::readInt32(int32_t *pArg) const
{
    if ((mDataPos+sizeof(int32_t)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += sizeof(int32_t);
        *pArg =  *reinterpret_cast<const int32_t*>(data);
        return NO_ERROR;
    } else {
        return NOT_ENOUGH_DATA;
    }
}

int32_t Parcel::readInt32() const
{
    if ((mDataPos+sizeof(int32_t)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += sizeof(int32_t);
        return *reinterpret_cast<const int32_t*>(data);
    }
    return 0;
}


status_t Parcel::readInt64(int64_t *pArg) const
{
    if ((mDataPos+sizeof(int64_t)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += sizeof(int64_t);
        *pArg = *reinterpret_cast<const int64_t*>(data);
        return NO_ERROR;
    } else {
        return NOT_ENOUGH_DATA;
    }
}


int64_t Parcel::readInt64() const
{
    if ((mDataPos+sizeof(int64_t)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += sizeof(int64_t);
        return *reinterpret_cast<const int64_t*>(data);
    }
    return 0;
}

status_t Parcel::readFloat(float *pArg) const
{
    if ((mDataPos+sizeof(float)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += sizeof(float);
        *pArg = *reinterpret_cast<const float*>(data);
        return NO_ERROR;
    } else {
        return NOT_ENOUGH_DATA;
    }
}


float Parcel::readFloat() const
{
    if ((mDataPos+sizeof(float)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += sizeof(float);
        return *reinterpret_cast<const float*>(data);
    }
    return 0;
}

status_t Parcel::readDouble(double *pArg) const
{
    if ((mDataPos+sizeof(double)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += sizeof(double);
        *pArg = *reinterpret_cast<const double*>(data);
        return NO_ERROR;
    } else {
        return NOT_ENOUGH_DATA;
    }
}


double Parcel::readDouble() const
{
    if ((mDataPos+sizeof(double)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += sizeof(double);
        return *reinterpret_cast<const double*>(data);
    }
    return 0;
}


const char* Parcel::readCString() const
{
    const size_t avail = mDataSize-mDataPos;
    if (avail > 0) {
        const char* str = reinterpret_cast<const char*>(mData+mDataPos);
        // is the string's trailing NUL within the parcel's valid bounds?
        const char* eos = reinterpret_cast<const char*>(memchr(str, 0, avail));
        if (eos) {
            const size_t len = eos - str;
            mDataPos += PAD_SIZE(len+1);
            return str;
        }
    }
    return NULL;
}

String8 Parcel::readString8() const
{
    int32_t size = readInt32();
    // watch for potential int overflow adding 1 for trailing NUL
    if (size > 0 && size < INT32_MAX) {
        const char* str = (const char*)readInplace(size+1);
        if (str) return String8(str, size);
    }
    return String8();
}

String16 Parcel::readString16() const
{
    size_t len;
    const char16_t* str = readString16Inplace(&len);
    if (str) return String16(str, len);
    return String16();
}

const char16_t* Parcel::readString16Inplace(size_t* outLen) const
{
    int32_t size = readInt32();
    // watch for potential int overflow from size+1
    if (size >= 0 && size < INT32_MAX) {
        *outLen = size;
        const char16_t* str = (const char16_t*)readInplace((size+1)*sizeof(char16_t));
        if (str != NULL) {
            return str;
        }
    }
    *outLen = 0;
    return NULL;
}

void Parcel::freeData()
{
    freeDataNoInit();
    initState();
}

void Parcel::freeDataNoInit()
{
    if (mOwner) {
        mOwner(this, mData, mDataSize, mObjects, mObjectsSize, mOwnerCookie);
    } else {
        if (mData) free(mData);
        if (mObjects) free(mObjects);
    }
}

status_t Parcel::growData(size_t len)
{
    size_t newSize = ((mDataSize+len)*3)/2;
    return (newSize <= mDataSize)
            ? (status_t) NO_MEMORY
            : continueWrite(newSize);
}

status_t Parcel::restartWrite(size_t desired)
{
    if (mOwner) {
        freeData();
        return continueWrite(desired);
    }
    
    uint8_t* data = (uint8_t*)realloc(mData, desired);
    if (!data && desired > mDataCapacity) {
        mError = NO_MEMORY;
        return NO_MEMORY;
    }
    
    if (data) {
        mData = data;
        mDataCapacity = desired;
    }
    
    mDataSize = mDataPos = 0;
        
    free(mObjects);
    mObjects = NULL;
    mObjectsSize = mObjectsCapacity = 0;
    mNextObjectHint = 0;
    mHasFds = false;
    mFdsKnown = true;
    
    return NO_ERROR;
}

status_t Parcel::continueWrite(size_t desired)
{
    // If shrinking, first adjust for any objects that appear
    // after the new data size.
    size_t objectsSize = mObjectsSize;
    if (desired < mDataSize) {
        if (desired == 0) {
            objectsSize = 0;
        } else {
            while (objectsSize > 0) {
                if (mObjects[objectsSize-1] < desired)
                    break;
                objectsSize--;
            }
        }
    }
    
    if (mOwner) {
        // If the size is going to zero, just release the owner's data.
        if (desired == 0) {
            freeData();
            return NO_ERROR;
        }

        // If there is a different owner, we need to take
        // posession.
        uint8_t* data = (uint8_t*)malloc(desired);
        if (!data) {
            mError = NO_MEMORY;
            return NO_MEMORY;
        }
        size_t* objects = NULL;
        
        if (objectsSize) {
            objects = (size_t*)malloc(objectsSize*sizeof(size_t));
            if (!objects) {
                mError = NO_MEMORY;
                return NO_MEMORY;
            }

            // Little hack to only acquire references on objects
            // we will be keeping.
            size_t oldObjectsSize = mObjectsSize;
            mObjectsSize = objectsSize;
            mObjectsSize = oldObjectsSize;
        }
        
        if (mData) {
            memcpy(data, mData, mDataSize < desired ? mDataSize : desired);
        }
        if (objects && mObjects) {
            memcpy(objects, mObjects, objectsSize*sizeof(size_t));
        }
        mOwner(this, mData, mDataSize, mObjects, mObjectsSize, mOwnerCookie);
        mOwner = NULL;

        mData = data;
        mObjects = objects;
        mDataSize = (mDataSize < desired) ? mDataSize : desired;
        mDataCapacity = desired;
        mObjectsSize = mObjectsCapacity = objectsSize;
        mNextObjectHint = 0;

    } else if (mData) {
        if (objectsSize < mObjectsSize) {
            size_t* objects =
                (size_t*)realloc(mObjects, objectsSize*sizeof(size_t));
            if (objects) {
                mObjects = objects;
            }
            mObjectsSize = objectsSize;
            mNextObjectHint = 0;
        }

        // We own the data, so we can just do a realloc().
        if (desired > mDataCapacity) {
            uint8_t* data = (uint8_t*)realloc(mData, desired);
            if (data) {
                mData = data;
                mDataCapacity = desired;
            } else if (desired > mDataCapacity) {
                mError = NO_MEMORY;
                return NO_MEMORY;
            }
        } else {
            mDataSize = desired;
            if (mDataPos > desired) {
                mDataPos = desired;
            }
        }
        
    } else {
        // This is the first data.  Easy!
        uint8_t* data = (uint8_t*)malloc(desired);
        if (!data) {
            mError = NO_MEMORY;
            return NO_MEMORY;
        }
        
        if(!(mDataCapacity == 0 && mObjects == NULL
             && mObjectsCapacity == 0)) {
        }
        
        mData = data;
        mDataSize = mDataPos = 0;
        mDataCapacity = desired;
    }

    return NO_ERROR;
}

void Parcel::initState()
{
    mError = NO_ERROR;
    mData = 0;
    mDataSize = 0;
    mDataCapacity = 0;
    mDataPos = 0;
    mObjects = NULL;
    mObjectsSize = 0;
    mObjectsCapacity = 0;
    mNextObjectHint = 0;
    mHasFds = false;
    mFdsKnown = true;
    mOwner = NULL;
}

}; // namespace android
