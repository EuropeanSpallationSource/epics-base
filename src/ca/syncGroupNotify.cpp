
/*
 *  $Id$
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#include "iocinf.h"

tsFreeList < class syncGroupNotify, 1024 > syncGroupNotify::freeList;
epicsMutex syncGroupNotify::freeListMutex;

syncGroupNotify::syncGroupNotify ( CASG &sgIn, void *pValueIn ) :
    sg (sgIn), magic (CASG_MAGIC), pValue (pValueIn), seqNo ( sgIn.seqNo )
{
    epicsAutoMutex locker ( this->sg.mutex );
    this->sg.ioList.add (*this);
    this->sg.opPendCount++;
}

void syncGroupNotify::release ()
{
    delete this;
}

syncGroupNotify::~syncGroupNotify ()
{
    epicsAutoMutex locker ( this->sg.mutex );
    this->sg.ioList.remove (*this);
}

void syncGroupNotify::completionNotify ( cacChannelIO & )
{
    bool done;

    if ( this->magic != CASG_MAGIC ) {
        ca_printf ("cac: sync group io_complete(): bad sync grp op magic number?\n");
        return;
    }

    {
        epicsAutoMutex locker ( this->sg.mutex );
        if ( this->seqNo == this->sg.seqNo ) {
            assert ( this->sg.opPendCount > 0u );
            this->sg.opPendCount--;
            done = this->sg.opPendCount == 0;
        }
        else {
            done = true;
        }
    }

    if ( done ) {
        this->sg.sem.signal ();
    }
}

void syncGroupNotify::completionNotify ( cacChannelIO &, 
    unsigned type, unsigned long count, const void *pData )
{
    if ( this->magic != CASG_MAGIC ) {
        ca_printf ("cac: sync group io_complete(): bad sync grp op magic number?\n");
        return;
    }

    bool complete;

    {
        epicsAutoMutex locker ( this->sg.mutex );
        if ( this->seqNo == this->sg.seqNo ) {
            /*
             * Update the user's variable
             */
            if ( this->pValue ) {
                size_t size = dbr_size_n ( type, count );
                memcpy ( this->pValue, pData, size );
            }
            assert ( this->sg.opPendCount > 0u );
            this->sg.opPendCount--;
            complete = this->sg.opPendCount == 0;
        }
        else {
            complete = true;
        }
    }

    if ( complete ) {
        this->sg.sem.signal ();
    }
}

void syncGroupNotify::show ( unsigned /* level */ ) const
{
    printf ( "pending sg op: pVal=%p, magic=%u seqNo=%lu sg=%p\n",
         this->pValue, this->magic, this->seqNo, 
         static_cast <void *> ( &this->sg ) );
}

void syncGroupNotify::exceptionNotify ( cacChannelIO &io,
    int status, const char *pContext )
{
    ca_signal_formated ( status, __FILE__, __LINE__, 
            "CA Sync Group request to channel %s failed because \"%s\"\n", 
            io.pName (), pContext);
}

void syncGroupNotify::exceptionNotify ( cacChannelIO &io,
    int status, const char *pContext, unsigned type, unsigned long count )
{
    ca_signal_formated ( status, __FILE__, __LINE__, 
            "CA Sync Group request failed with channel=%s type=%d count=%ld because \"%s\"\n", 
            io.pName (), type, count, pContext);
}

void * syncGroupNotify::operator new (size_t size)
{
    return syncGroupNotify::freeList.allocate ( size );
}

void syncGroupNotify::operator delete (void *pCadaver, size_t size)
{
    syncGroupNotify::freeList.release ( pCadaver, size );
}


