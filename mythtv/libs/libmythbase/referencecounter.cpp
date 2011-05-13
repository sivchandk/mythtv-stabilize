// -*- Mode: c++ -*-

#include <QMutexLocker>
#include "referencecounter.h"

ReferenceCounter::ReferenceCounter(void) :
    m_refCount(1)
{
}

void ReferenceCounter::UpRef(void)
{
    QMutexLocker mlock(&m_refLock);
    m_refCount++;
}

bool ReferenceCounter::DownRef(void)
{
    QMutexLocker mlock(&m_refLock);
    if (--m_refCount == 0)
    {
        delete this;
        return true;
    }

    return false;
}

ReferenceLocker::ReferenceLocker(ReferenceCounter *counter, bool upref) :
    m_refObject(counter)
{
    if (upref)
        m_refObject->UpRef();
}

ReferenceLocker::~ReferenceLocker(void)
{
    m_refObject->DownRef();
    m_refObject = NULL;
}
