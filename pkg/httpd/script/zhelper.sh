#!/bin/ksh

/bin/xhelper x

# n-jos64
/bin/dhelper 171.66.3.149 user 0

# class1
/bin/dhelper 171.66.3.191 app 0

# class3
/bin/dhelper 171.66.3.193 app 1

# class4 
#/bin/dhelper 171.66.3.194 app 1
/bin/dhelper 171.66.3.194 app 2

# barf
#/bin/dhelper 171.66.3.239 app 2
#/bin/dhelper 171.66.3.239 app 3
/bin/dhelper 171.66.3.239 user 1
