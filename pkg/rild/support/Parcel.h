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

#ifndef ANDROID_PARCEL_H
#define ANDROID_PARCEL_H

#include "String16.h"

// ---------------------------------------------------------------------------
namespace android {

class String8;

class Parcel
{
public:
                        Parcel();
                        ~Parcel();
    
    const uint8_t*      data() const;
    size_t              dataSize() const;
    size_t              dataAvail() const;
    size_t              dataPosition() const;
    size_t              dataCapacity() const;
    
    status_t            setDataSize(size_t size);
    void                setDataPosition(size_t pos) const;
    status_t            setDataCapacity(size_t size);
    
    status_t            setData(const uint8_t* buffer, size_t len);

    status_t            writeInterfaceToken(const String16& interface);
    bool                enforceInterface(const String16& interface) const;
            
    void                freeData();

    const size_t*       objects() const;
    size_t              objectsCount() const;
    
    status_t            errorCheck() const;
    void                setError(status_t err);
    
    status_t            write(const void* data, size_t len);
    void*               writeInplace(size_t len);
    status_t            writeUnpadded(const void* data, size_t len);
    status_t            writeInt32(int32_t val);
    status_t            writeInt64(int64_t val);
    status_t            writeFloat(float val);
    status_t            writeDouble(double val);
    status_t            writeCString(const char* str);
    status_t            writeString8(const String8& str);
    status_t            writeString16(const String16& str);
    status_t            writeString16(const char16_t* str, size_t len);

    // Place a file descriptor into the parcel.  A dup of the fd is made, which
    // will be closed once the parcel is destroyed.
    
    void                remove(size_t start, size_t amt);
    
    status_t            read(void* outData, size_t len) const;
    const void*         readInplace(size_t len) const;
    int32_t             readInt32() const;
    status_t            readInt32(int32_t *pArg) const;
    int64_t             readInt64() const;
    status_t            readInt64(int64_t *pArg) const;
    float               readFloat() const;
    status_t            readFloat(float *pArg) const;
    double              readDouble() const;
    status_t            readDouble(double *pArg) const;

    const char*         readCString() const;
    String8             readString8() const;
    String16            readString16() const;
    const char16_t*     readString16Inplace(size_t* outLen) const;

    typedef void        (*release_func)(Parcel* parcel,
                                        const uint8_t* data, size_t dataSize,
                                        const size_t* objects, size_t objectsSize,
                                        void* cookie);
                        
private:
                        Parcel(const Parcel& o);
    Parcel&             operator=(const Parcel& o);
    
    status_t            finishWrite(size_t len);
    status_t            growData(size_t len);
    status_t            restartWrite(size_t desired);
    status_t            continueWrite(size_t desired);
    void                freeDataNoInit();
    void                initState();
                        
    status_t            mError;
    uint8_t*            mData;
    size_t              mDataSize;
    size_t              mDataCapacity;
    mutable size_t      mDataPos;
    size_t*             mObjects;
    size_t              mObjectsSize;
    size_t              mObjectsCapacity;
    mutable size_t      mNextObjectHint;

    mutable bool        mFdsKnown;
    mutable bool        mHasFds;
    
    release_func        mOwner;
    void*               mOwnerCookie;
};

}; // namespace android

// ---------------------------------------------------------------------------

#endif // ANDROID_PARCEL_H