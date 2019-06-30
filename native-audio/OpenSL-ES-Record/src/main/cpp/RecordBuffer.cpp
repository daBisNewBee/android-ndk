//
// Created by ywl on 2018/3/27.
//

#include "RecordBuffer.h"
#include "AndroidLog.h"

RecordBuffer::RecordBuffer(int buffersize) {
    buffer = new short *[2];
    for(int i = 0; i < 2; i++)
    {
        buffer[i] = new short[buffersize];
    }
}

RecordBuffer::~RecordBuffer() {
}

short *RecordBuffer::getRecordBuffer() {
    index++;
    if(index > 1)
    {
        index = 0;
    }
    LOGD("getRecordBuffer:%d ", index);
    return buffer[index];
}

short * RecordBuffer::getNowBuffer() {
    LOGD("getNowBuffer:%d ", index);
    return buffer[index];
}
