/*
 *  BufferManagerTest.cpp
 *  MWorksCore
 *
 *  Created by Ben Kennedy 2006
 *  Copyright 2006 MIT. All rights reserved.
 *
 */
/*
#include "BufferManagerTest.h"
#include "MWorksCore/EventBuffer.h"
#include "MWorksCore/Event.h"
#include "MWorksCore/GenericData.h"
#include <pthread.h>

#define NUM_EVENTS 1500
#define EVENT_CODE 1
using namespace mw;


struct bmtTestArgs {
	int testNumber;
	bool runningFlag;
};

const int bmt_TIMEOUT_TIME = 2400;
const int bmt_NUM_THREADS = 10;
const int bmt_NUM_EVENTS_PER_THREAD = NUM_EVENTS;

boost::mutex bmt_cppunit_lock;
struct bmtTestArgs *bmt_ptrTestArgs;
bool bmt_Asserted;
std::string bmt_AssertMessage;
shared_ptr<BufferManager> bmtTestglobal_outgoing_event_buffer;
std::vector<shared_ptr<EventBufferReader> >eventReaders;
int totalEventsRead;
boost::mutex terLock;

void *bmtMassEventWrite(void *args);
void *bmtMassEventRead(void *args);
void *bmtTimeoutThread(void *args);
void bmt_assert(const std::string &message,
				const bool assertCondition);


CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( BufferManagerTestFixture, "Unit Test" );

void BufferManagerTestFixture::setUp() {
	shared_ptr <Clock> shared_clock = Clock::instance(false);
	CPPUNIT_ASSERT(shared_clock == NULL);
	if(shared_clock == NULL) {
		shared_ptr <Clock> new_clock = shared_ptr<Clock>(new Clock(0));
		Clock::registerInstance(new_clock);
	}
	shared_clock = Clock::instance(false);
	CPPUNIT_ASSERT(shared_clock != NULL);
	
	
	static int testNumber = 0;
	bmt_ptrTestArgs = new struct bmtTestArgs;
	bmt_ptrTestArgs->testNumber = testNumber;
	bmt_ptrTestArgs->runningFlag = true;
	testNumber++;
	bmt_AssertMessage = "";
	bmt_Asserted = false;
	bmtTestglobal_outgoing_event_buffer = shared_ptr<BufferManager>(new BufferManager());
	
	
	
	pthread_t timeout;
	pthread_create(&timeout, NULL, &bmtTimeoutThread, (void *)bmt_ptrTestArgs);	
}

void BufferManagerTestFixture::tearDown() {
	boost::mutex::scoped_lock lock(bmt_cppunit_lock);
	shared_ptr <Clock> shared_clock = Clock::instance(false);
	CPPUNIT_ASSERT(shared_clock != 0);
	
	bmt_ptrTestArgs->runningFlag = false;
	eventReaders.clear();
	CPPUNIT_ASSERT_MESSAGE(bmt_AssertMessage, !bmt_Asserted);

	Clock::destroy();

	shared_clock = Clock::instance(false);
	CPPUNIT_ASSERT(shared_clock == NULL);
}

void BufferManagerTestFixture::testSimpleBufferManager() {	
	shared_ptr<BufferManager> bm = shared_ptr<BufferManager>(new BufferManager());
	
	shared_ptr<EventBufferReader> mbr = bm->getNewMainBufferReader();
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	
	for (int i=0; i<NUM_EVENTS; ++i) {
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(i));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(i+1));
		shared_ptr<Event> evt=shared_ptr<Event>(new Event(EVENT_CODE, Datum(M_INTEGER, i)));
		bm->putEvent(evt);
		
		CPPUNIT_ASSERT(mbr->nextEventExists());
	}
	
	for (int i=0; i<NUM_EVENTS; ++i) {
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i+1));
		CPPUNIT_ASSERT(mbr->nextEventExists());
		shared_ptr<Event> evt = mbr->getNextEvent();
		CPPUNIT_ASSERT(evt != 0);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		CPPUNIT_ASSERT(evt->getData().isInteger());
		
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i-1));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i));
	}		
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	shared_ptr<Event> evt = mbr->getNextEvent();
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr->nextEventExists());
}

void BufferManagerTestFixture::testForLeaking() {	
	shared_ptr<BufferManager> bm = shared_ptr<BufferManager>(new BufferManager());
	shared_ptr<EventBufferReader> mbr = bm->getNewMainBufferReader();
	
	{
		shared_ptr<Event> evt=shared_ptr<Event>(new Event(EVENT_CODE, Datum(M_INTEGER, 1)));
		bm->putEvent(evt);
	}
	CPPUNIT_ASSERT(mbr->nextEventExists());
	
	
	shared_ptr<Event> strongEvt = mbr->getNextEvent();
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	weak_ptr<Event>weakEvt(strongEvt);
	CPPUNIT_ASSERT(!weakEvt.expired());
	// 1 for strongEvt, 
	// 1 for the currentEvent in the buffer reader, 
	// 1 for the headPtr in the buffer manager
	CPPUNIT_ASSERT(weakEvt.use_count() == 3);
	
	
	
	// add another event
	{
		shared_ptr<Event> evt=shared_ptr<Event>(new Event(EVENT_CODE, Datum(M_INTEGER, 1)));
		bm->putEvent(evt);
	}
	CPPUNIT_ASSERT(mbr->nextEventExists());
	
	CPPUNIT_ASSERT(!weakEvt.expired());
	// 1 for the currentEvent in the buffer reader, and 1 for the strongPtr 
	CPPUNIT_ASSERT(weakEvt.use_count() == 2);		
	
	mbr->getNextEvent();
	
	CPPUNIT_ASSERT(!weakEvt.expired());
	// 1 for the strongPtr() 
	CPPUNIT_ASSERT(weakEvt.use_count() == 1);		
}

void BufferManagerTestFixture::testForLeaking2() {	
	shared_ptr<BufferManager> bm = shared_ptr<BufferManager>(new BufferManager());
	shared_ptr<EventBufferReader> mbr = bm->getNewMainBufferReader();
	
	shared_ptr<Event> evt=shared_ptr<Event>(new Event(EVENT_CODE, Datum(M_INTEGER, 1)));
	weak_ptr<Event>weakEvt(evt);
	CPPUNIT_ASSERT(weakEvt.use_count() == 1);

	bm->putEvent(evt);
	// headPtr + 3vt + currentEvent->next
	CPPUNIT_ASSERT(weakEvt.use_count() == 3);
	
	// DDC added
	evt = shared_ptr<Event>(new Event(EVENT_CODE, Datum(M_INTEGER, 1)));
	
	// the original evt should have fallen out of this scope
	CPPUNIT_ASSERT(weakEvt.use_count() == 2);
	
	
	mbr->getNextEvent();
	// 1 for the currentEvent in the buffer reader, 
	// 1 for the headPtr in the buffer manager
	// 1 for evt
	CPPUNIT_ASSERT(weakEvt.use_count() == 2);
	
	
	
	// add another event
	shared_ptr <Event> newevent(new Event(EVENT_CODE, Datum(M_INTEGER, 1)));
	bm->putEvent(newevent);
	// 1 for the currentEvent in the buffer reader, 
	// 1 for evt
	CPPUNIT_ASSERT(weakEvt.use_count() == 1);
	CPPUNIT_ASSERT(mbr->nextEventExists());
	
	mbr->getNextEvent();
	
	// 1 for evt
	CPPUNIT_ASSERT(weakEvt.expired());
}

void BufferManagerTestFixture::testForLeaking3() {
	shared_ptr<EventBufferReader> mbr2 = bmtTestglobal_outgoing_event_buffer->getNewMainBufferReader();
	const int totalEvents = bmt_NUM_EVENTS_PER_THREAD*bmt_NUM_THREADS;
	
	
	{
		shared_ptr<EventBufferReader> mbr1 = bmtTestglobal_outgoing_event_buffer->getNewMainBufferReader();
		CPPUNIT_ASSERT(!mbr1->nextEventExists());
		CPPUNIT_ASSERT(!mbr2->nextEventExists());
		
		pthread_t threads[bmt_NUM_THREADS];
		pthread_attr_t pthread_custom_attr;
		
		pthread_attr_init(&pthread_custom_attr);
		
    int to_send[bmt_NUM_THREADS];
		for (int i=0; i<bmt_NUM_THREADS; ++i){
      to_send[i] = i;
			CPPUNIT_ASSERT(pthread_create(&threads[i], 
										  &pthread_custom_attr, 
										  &bmtMassEventWrite, (void *)(&to_send[i])) == 0);
		}
		
		int numEventsRead = 0;
		
		while(numEventsRead < totalEvents) {
			if(mbr1->nextEventExists()) {
				++numEventsRead;
				shared_ptr<Event> evt = mbr1->getNextEvent();
				CPPUNIT_ASSERT(evt);
				CPPUNIT_ASSERT(evt->getData().isInteger());
//				if(numEventsRead == totalEvents) {
//					CPPUNIT_ASSERT(evt.use_count() == 4);
//				} else {
//					int shizzy = evt.use_count();
//					if(shizzy != 3) {
//						int crap=0;
//					}
//					CPPUNIT_ASSERT(evt.use_count() == 3);
//				}
				
			} else {
				//fprintf(stderr, "blocking numEventsRead = %d\n", numEventsRead); 
			}
		}
		
		for (int i=0; i<bmt_NUM_THREADS; i++)
		{
			CPPUNIT_ASSERT(pthread_join(threads[i], NULL) == 0);
		}
	}
	
	
	int x = 0;
	while(mbr2->nextEventExists()) {
		x++;
		shared_ptr<Event> lastEvent = mbr2->getNextEvent();
		if(x == totalEvents) {
			CPPUNIT_ASSERT(lastEvent.use_count() == 3);
		} else {
			CPPUNIT_ASSERT(lastEvent.use_count() == 2);
		}
	}
}


void BufferManagerTestFixture::testMultipleReaders() {	
	shared_ptr<BufferManager> bm = shared_ptr<BufferManager>(new BufferManager());
	
	shared_ptr<EventBufferReader> mbr = bm->getNewMainBufferReader();
	shared_ptr<EventBufferReader> mbr2;
	shared_ptr<EventBufferReader> mbr3;
	shared_ptr<EventBufferReader> mbr4 = bm->getNewMainBufferReader();
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	
	for (int i=0; i<NUM_EVENTS; ++i) {
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
		CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(i));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(i+1));
		shared_ptr<Event> evt=shared_ptr<Event>(new Event(EVENT_CODE, Datum(M_INTEGER, i)));
		CPPUNIT_ASSERT(evt);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().isInteger());				
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		bm->putEvent(evt);
		
		CPPUNIT_ASSERT(mbr->nextEventExists());
		CPPUNIT_ASSERT(mbr4->nextEventExists());
		
		evt = mbr4->getNextEvent();
		CPPUNIT_ASSERT(evt);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().isInteger());				
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
		
		if(i == NUM_EVENTS/2) {
			mbr2 = bm->getNewMainBufferReader();
		}
		
		if(i>=NUM_EVENTS/2) {
			mbr3 = bm->getNewMainBufferReader();
			CPPUNIT_ASSERT(!mbr3->nextEventExists());
			CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(i-(NUM_EVENTS/2)));
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents((i-(NUM_EVENTS/2))+1));
		}
		
	}
	
	CPPUNIT_ASSERT(mbr->nextEventExists());
	CPPUNIT_ASSERT(mbr2->nextEventExists());
	CPPUNIT_ASSERT(!mbr3->nextEventExists());
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	
	
	CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS));
	CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS+1));
	CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(NUM_EVENTS/2-1));
	CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents((NUM_EVENTS/2)));
	CPPUNIT_ASSERT(!mbr3->hasAtLeastNEvents(1));
	CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
	
	
	for (int i=0; i<NUM_EVENTS; ++i) {
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i+1));
		CPPUNIT_ASSERT(mbr->nextEventExists());
		shared_ptr<Event> evt = mbr->getNextEvent();
		CPPUNIT_ASSERT(evt != 0);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().isInteger());				
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i-1));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i));
		
		if(i<NUM_EVENTS/2-1) {
			CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i-1));
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i));
			CPPUNIT_ASSERT(mbr2->nextEventExists());
		} else {
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(1));
			CPPUNIT_ASSERT(!mbr2->nextEventExists());
		}
		evt = mbr2->getNextEvent();
		if(i<NUM_EVENTS/2-1) {
			CPPUNIT_ASSERT(evt);
			CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
			CPPUNIT_ASSERT(evt->getData().isInteger());				
			CPPUNIT_ASSERT(evt->getData().getInteger() == i+NUM_EVENTS/2+1);
			CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i-2));
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i-1));
		} else {
			CPPUNIT_ASSERT(evt == 0);
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(1));
			CPPUNIT_ASSERT(!mbr2->nextEventExists());
		}
		
		CPPUNIT_ASSERT(!mbr3->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr3->nextEventExists());
		evt = mbr3->getNextEvent();
		CPPUNIT_ASSERT(evt == 0);			
		CPPUNIT_ASSERT(!mbr3->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr3->nextEventExists());
		
		CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
		evt = mbr4->getNextEvent();
		CPPUNIT_ASSERT(evt == 0);			
		CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
	}		
	
	shared_ptr<Event> evt;
	evt = mbr->getNextEvent();
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	evt = mbr2->getNextEvent();
	CPPUNIT_ASSERT(!mbr2->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr2->nextEventExists());
	evt = mbr3->getNextEvent();
	CPPUNIT_ASSERT(!mbr3->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr3->nextEventExists());
	evt = mbr4->getNextEvent();
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	
	
}

void BufferManagerTestFixture::testMultipleReadersWithAddtionalPuts() {	
	shared_ptr<BufferManager> bm = shared_ptr<BufferManager>(new BufferManager());
	
	shared_ptr<EventBufferReader> mbr = bm->getNewMainBufferReader();
	shared_ptr<EventBufferReader> mbr2;
	shared_ptr<EventBufferReader> mbr3;
	shared_ptr<EventBufferReader> mbr4 = bm->getNewMainBufferReader();
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	
	for (int i=0; i<NUM_EVENTS; ++i) {
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
		CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(i));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(i+1));
		shared_ptr<Event> evt=shared_ptr<Event>(new Event(EVENT_CODE, Datum(M_INTEGER, i)));
		CPPUNIT_ASSERT(evt);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().isInteger());				
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		bm->putEvent(evt);
		
		CPPUNIT_ASSERT(mbr->nextEventExists());
		CPPUNIT_ASSERT(mbr4->nextEventExists());
		
		evt = mbr4->getNextEvent();
		CPPUNIT_ASSERT(evt);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().isInteger());				
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
		
		if(i == NUM_EVENTS/2) {
			mbr2 = bm->getNewMainBufferReader();
		}
		
		if(i>=NUM_EVENTS/2) {
			mbr3 = bm->getNewMainBufferReader();
			CPPUNIT_ASSERT(!mbr3->nextEventExists());
			CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(i-(NUM_EVENTS/2)));
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents((i-(NUM_EVENTS/2))+1));
		}
		
	}
	
	CPPUNIT_ASSERT(mbr->nextEventExists());
	CPPUNIT_ASSERT(mbr2->nextEventExists());
	CPPUNIT_ASSERT(!mbr3->nextEventExists());
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	
	
	CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS));
	CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS+1));
	CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(NUM_EVENTS/2-1));
	CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents((NUM_EVENTS/2)));
	CPPUNIT_ASSERT(!mbr3->hasAtLeastNEvents(1));
	CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
	
	
	for (int i=0; i<NUM_EVENTS; ++i) {
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i+1));
		CPPUNIT_ASSERT(mbr->nextEventExists());
		shared_ptr<Event> evt = mbr->getNextEvent();
		CPPUNIT_ASSERT(evt != 0);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().isInteger());				
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i-1));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i));
		
		if(i<NUM_EVENTS/2-1) {
			CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i-1));
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i));
			CPPUNIT_ASSERT(mbr2->nextEventExists());
		} else {
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(1));
			CPPUNIT_ASSERT(!mbr2->nextEventExists());
		}
		evt = mbr2->getNextEvent();
		if(i<NUM_EVENTS/2-1) {
			CPPUNIT_ASSERT(evt);
			CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
			CPPUNIT_ASSERT(evt->getData().isInteger());				
			CPPUNIT_ASSERT(evt->getData().getInteger() == i+NUM_EVENTS/2+1);
			CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i-2));
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i-1));
		} else {
			CPPUNIT_ASSERT(evt == 0);
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(1));
			CPPUNIT_ASSERT(!mbr2->nextEventExists());
		}
		
		CPPUNIT_ASSERT(!mbr3->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr3->nextEventExists());
		evt = mbr3->getNextEvent();
		CPPUNIT_ASSERT(evt == 0);			
		CPPUNIT_ASSERT(!mbr3->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr3->nextEventExists());
		
		CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
		evt = mbr4->getNextEvent();
		CPPUNIT_ASSERT(evt == 0);			
		CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
	}		
	
	shared_ptr<Event> evt;
	evt = mbr->getNextEvent();
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	evt = mbr2->getNextEvent();
	CPPUNIT_ASSERT(!mbr2->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr2->nextEventExists());
	evt = mbr3->getNextEvent();
	CPPUNIT_ASSERT(!mbr3->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr3->nextEventExists());
	evt = mbr4->getNextEvent();
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	
	for (int i=0; i<NUM_EVENTS; ++i) {
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
		CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(i));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(i+1));
		shared_ptr<Event> evt=shared_ptr<Event>(new Event(EVENT_CODE, Datum(M_INTEGER, i)));
		CPPUNIT_ASSERT(evt);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().isInteger());				
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		bm->putEvent(evt);
		
		CPPUNIT_ASSERT(mbr->nextEventExists());
		CPPUNIT_ASSERT(mbr4->nextEventExists());
		
		evt = mbr4->getNextEvent();
		CPPUNIT_ASSERT(evt);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().isInteger());				
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
		
		if(i == NUM_EVENTS/2) {
			mbr2 = bm->getNewMainBufferReader();
		}
		
		if(i>=NUM_EVENTS/2) {
			mbr3 = bm->getNewMainBufferReader();
			CPPUNIT_ASSERT(!mbr3->nextEventExists());
			CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(i-(NUM_EVENTS/2)));
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents((i-(NUM_EVENTS/2))+1));
		}
		
	}
	
	CPPUNIT_ASSERT(mbr->nextEventExists());
	CPPUNIT_ASSERT(mbr2->nextEventExists());
	CPPUNIT_ASSERT(!mbr3->nextEventExists());
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	
	
	CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS));
	CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS+1));
	CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(NUM_EVENTS/2-1));
	CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents((NUM_EVENTS/2)));
	CPPUNIT_ASSERT(!mbr3->hasAtLeastNEvents(1));
	CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
	
	
	for (int i=0; i<NUM_EVENTS; ++i) {
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i+1));
		CPPUNIT_ASSERT(mbr->nextEventExists());
		shared_ptr<Event> evt = mbr->getNextEvent();
		CPPUNIT_ASSERT(evt != 0);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().isInteger());				
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i-1));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i));
		
		if(i<NUM_EVENTS/2-1) {
			CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i-1));
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i));
			CPPUNIT_ASSERT(mbr2->nextEventExists());
		} else {
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(1));
			CPPUNIT_ASSERT(!mbr2->nextEventExists());
		}
		evt = mbr2->getNextEvent();
		if(i<NUM_EVENTS/2-1) {
			CPPUNIT_ASSERT(evt);
			CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
			CPPUNIT_ASSERT(evt->getData().isInteger());				
			CPPUNIT_ASSERT(evt->getData().getInteger() == i+NUM_EVENTS/2+1);
			CPPUNIT_ASSERT(mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i-2));
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(NUM_EVENTS/2-i-1));
		} else {
			CPPUNIT_ASSERT(evt == 0);
			CPPUNIT_ASSERT(!mbr2->hasAtLeastNEvents(1));
			CPPUNIT_ASSERT(!mbr2->nextEventExists());
		}
		
		CPPUNIT_ASSERT(!mbr3->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr3->nextEventExists());
		evt = mbr3->getNextEvent();
		CPPUNIT_ASSERT(evt == 0);			
		CPPUNIT_ASSERT(!mbr3->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr3->nextEventExists());
		
		CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
		evt = mbr4->getNextEvent();
		CPPUNIT_ASSERT(evt == 0);			
		CPPUNIT_ASSERT(!mbr4->hasAtLeastNEvents(1));
		CPPUNIT_ASSERT(!mbr4->nextEventExists());
	}		
	
	evt = mbr->getNextEvent();
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	evt = mbr2->getNextEvent();
	CPPUNIT_ASSERT(!mbr2->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr2->nextEventExists());
	evt = mbr3->getNextEvent();
	CPPUNIT_ASSERT(!mbr3->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr3->nextEventExists());
	evt = mbr4->getNextEvent();
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr4->nextEventExists());
	
}

void BufferManagerTestFixture::testBufferManagerDeletion() {	
	shared_ptr<EventBufferReader> mbr;
	
	{
		shared_ptr<BufferManager> bm = shared_ptr<BufferManager>(new BufferManager());
		mbr = bm->getNewMainBufferReader();
		CPPUNIT_ASSERT(!mbr->nextEventExists());
		
		for (int i=0; i<NUM_EVENTS; ++i) {
			CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(i));
			CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(i+1));
			shared_ptr<Event> evt=shared_ptr<Event>(new Event(EVENT_CODE, Datum(M_INTEGER, i)));
			bm->putEvent(evt);
			
			CPPUNIT_ASSERT(mbr->nextEventExists());
		}
		
		// OK now delete the buffer manager
	}
	
	for (int i=0; i<NUM_EVENTS; ++i) {
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i+1));
		CPPUNIT_ASSERT(mbr->nextEventExists());
		shared_ptr<Event> evt = mbr->getNextEvent();
		CPPUNIT_ASSERT(evt != 0);
		CPPUNIT_ASSERT(evt->getEventCode() == EVENT_CODE);
		CPPUNIT_ASSERT(evt->getData().getInteger() == i);
		CPPUNIT_ASSERT(evt->getData().isInteger());
		
		CPPUNIT_ASSERT(mbr->hasAtLeastNEvents(NUM_EVENTS-i-1));
		CPPUNIT_ASSERT(!mbr->hasAtLeastNEvents(NUM_EVENTS-i));
	}		
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	shared_ptr<Event> evt = mbr->getNextEvent();
	CPPUNIT_ASSERT(evt == 0);
	CPPUNIT_ASSERT(!mbr->nextEventExists());
}	




void BufferManagerTestFixture::multiThreadInsertTest() {	
	const int totalEvents = bmt_NUM_EVENTS_PER_THREAD*bmt_NUM_THREADS;
	
	shared_ptr<EventBufferReader> mbr = bmtTestglobal_outgoing_event_buffer->getNewMainBufferReader();
	CPPUNIT_ASSERT(!mbr->nextEventExists());
	
	pthread_t threads[bmt_NUM_THREADS];
	pthread_attr_t pthread_custom_attr;
	
	pthread_attr_init(&pthread_custom_attr);
	
  int to_send[bmt_NUM_THREADS];
	for (int i=0; i<bmt_NUM_THREADS; ++i)
	{
    to_send[i] = i;
		CPPUNIT_ASSERT(pthread_create(&threads[i], 
									  &pthread_custom_attr, 
									  &bmtMassEventWrite, (void *)(&to_send[i])) == 0);
	}
	
	int numEventsRead = 0;
	
	std::vector<int> eventCodeReceived[bmt_NUM_THREADS];
	while(numEventsRead < totalEvents) {
		if(mbr->nextEventExists()) {
			shared_ptr<Event> evt = mbr->getNextEvent();
			CPPUNIT_ASSERT(evt);
			CPPUNIT_ASSERT(evt->getData().isInteger());
			
			eventCodeReceived[evt->getEventCode()].push_back(evt->getData().getInteger());			
			++numEventsRead;
		}
	}
	
	for (int i=0; i<bmt_NUM_THREADS; i++)
	{
		CPPUNIT_ASSERT(pthread_join(threads[i], NULL) == 0);
	}	
	
	for(int i = 0; i<bmt_NUM_THREADS; ++i) {
		int currValue = -1;
		for(std::vector<int>::iterator j=eventCodeReceived[i].begin();
			j!=eventCodeReceived[i].end();
			++j) {
			
			CPPUNIT_ASSERT(currValue+1 == *j);
			currValue = *j;
		}	
	}
	
}

void BufferManagerTestFixture::multiThreadReadTest() {	
	const int totalEventsToRead = bmt_NUM_EVENTS_PER_THREAD*bmt_NUM_THREADS*bmt_NUM_THREADS;
	totalEventsRead = 0;
	
	for (int i =0; i<bmt_NUM_THREADS; ++i) {
		// prepare all of the readers
		eventReaders.push_back(bmtTestglobal_outgoing_event_buffer->getNewMainBufferReader());
		CPPUNIT_ASSERT(!eventReaders[i]->nextEventExists());
	}
	
	
	for(int i = 0; i<bmt_NUM_THREADS; i++) {
		for(int j = 0; j<bmt_NUM_EVENTS_PER_THREAD; j++) {
			
		 Datum _j(M_INTEGER, j );	
			shared_ptr<Event> newevent(new Event(i, _j));
			bmtTestglobal_outgoing_event_buffer->putEvent(newevent);
		}
	}
	
  int int_data[bmt_NUM_THREADS];
  
  for(int i = 0; i < bmt_NUM_THREADS; i++){
    int_data[i] = i;
  }
	pthread_t threads[bmt_NUM_THREADS];
	pthread_attr_t pthread_custom_attr;
	
	pthread_attr_init(&pthread_custom_attr);
	
	for (int i=0; i<bmt_NUM_THREADS; ++i)
	{
		CPPUNIT_ASSERT(pthread_create(&threads[i], 
									  &pthread_custom_attr, 
									  &bmtMassEventRead, (void *)(&int_data[i])) == 0);
	}
	
	for (int i=0; i<bmt_NUM_THREADS; i++)
	{
		CPPUNIT_ASSERT(pthread_join(threads[i], NULL) == 0);
	}
	
	for (int i =0; i<bmt_NUM_THREADS; ++i) {
		CPPUNIT_ASSERT(!eventReaders[i]->nextEventExists());
	}
	
	CPPUNIT_ASSERT(totalEventsToRead == totalEventsRead);
}

void BufferManagerTestFixture::multiMultiTest() {	
	const int totalEventsToRead = (bmt_NUM_EVENTS_PER_THREAD*bmt_NUM_THREADS*bmt_NUM_THREADS);
	totalEventsRead = 0;
	
	for (int i =0; i<bmt_NUM_THREADS; ++i) {
		// prepare all of the readers
		eventReaders.push_back(bmtTestglobal_outgoing_event_buffer->getNewMainBufferReader());
		CPPUNIT_ASSERT(!eventReaders[i]->nextEventExists());
	}
	
	// plus one bonus reader
	shared_ptr<EventBufferReader> mbr = bmtTestglobal_outgoing_event_buffer->getNewMainBufferReader();
	
	pthread_t readThreads[bmt_NUM_THREADS];
	pthread_t writeThreads[bmt_NUM_THREADS];
	pthread_attr_t pthread_custom_attr;
	
	pthread_attr_init(&pthread_custom_attr);
	
  int to_send[bmt_NUM_THREADS];
  
	// start the readers and writers
	for (int i=0; i<bmt_NUM_THREADS; ++i)
	{
    to_send[i] = i;
		CPPUNIT_ASSERT(pthread_create(&readThreads[i], 
									  &pthread_custom_attr, 
									  &bmtMassEventRead, (void *)(&to_send[i])) == 0);
		CPPUNIT_ASSERT(pthread_create(&writeThreads[i], 
									  &pthread_custom_attr, 
									  &bmtMassEventWrite, (void *)(&to_send[i])) == 0);
	}
	
	int numEventsRead = 0;
	
	std::vector<int> eventCodeReceived[bmt_NUM_THREADS];
	while(numEventsRead < bmt_NUM_EVENTS_PER_THREAD*bmt_NUM_THREADS) {
		if(mbr->nextEventExists()) {
			shared_ptr<Event> evt = mbr->getNextEvent();
			CPPUNIT_ASSERT(evt);
			CPPUNIT_ASSERT(evt->getData().isInteger());
			
			eventCodeReceived[evt->getEventCode()].push_back(evt->getData().getInteger());			
			++numEventsRead;
		}
	}
	
	for (int i=0; i<bmt_NUM_THREADS; i++)
	{
		CPPUNIT_ASSERT(pthread_join(writeThreads[i], NULL) == 0);
		CPPUNIT_ASSERT(pthread_join(readThreads[i], NULL) == 0);
	}
	
	for (int i =0; i<bmt_NUM_THREADS; ++i) {
		CPPUNIT_ASSERT(!eventReaders[i]->nextEventExists());
	}
	
	
	for(int i = 0; i<bmt_NUM_THREADS; ++i) {
		int currValue = -1;
		for(std::vector<int>::iterator j=eventCodeReceived[i].begin();
			j!=eventCodeReceived[i].end();
			++j) {
			
			CPPUNIT_ASSERT(currValue+1 == *j);
			currValue = *j;
		}	
	}
	
	CPPUNIT_ASSERT(totalEventsToRead == totalEventsRead);
}

void *bmtMassEventWrite(void *args) {
  int *int_arg = (int *)args;
  
	for (int i =0; i<bmt_NUM_EVENTS_PER_THREAD; ++i) {
		//fprintf(stderr, "Thread %d adding %d\n", (int)args, i);
		
	 Datum _i(M_INTEGER, i);
		
		//shared_ptr<Event> newevent(new Event((int)args, _i));
		shared_ptr<Event> newevent(new Event(*int_arg, _i));
    
    bmtTestglobal_outgoing_event_buffer->putEvent(newevent);
	}
	
	return NULL;
}

void bmt_assert(const std::string &message, 
				const bool assertCondition) {
	boost::mutex::scoped_lock lock(bmt_cppunit_lock); 
	if(!assertCondition && !bmt_Asserted) {
		bmt_Asserted = true;
		bmt_AssertMessage = message;
	}
}
void *bmtTimeoutThread(void *args) {
	struct bmtTestArgs *ta = (struct bmtTestArgs *)args;	
	sleep(bmt_TIMEOUT_TIME);
	if(ta->runningFlag) {
		bool timeout = false;
		bmt_assert("Timed out", timeout);
	} 
	
	pthread_detach(pthread_self());	
	delete ta;
	
	return 0;
}

void *bmtMassEventRead(void *args) {
	int numEventsRead = 0;
	int *int_arg = (int *)args;
  
	shared_ptr<EventBufferReader> mbr = eventReaders[*int_arg];
	
	std::vector<int> eventCodeReceived[bmt_NUM_THREADS];
	while(numEventsRead < bmt_NUM_EVENTS_PER_THREAD*bmt_NUM_THREADS) {
		if(mbr->nextEventExists()) {
			shared_ptr<Event> evt = mbr->getNextEvent();
			bmt_assert("bad event", evt);
			bmt_assert("event doesn't contain an integer", 
					   evt->getData().isInteger());
			
			eventCodeReceived[evt->getEventCode()].push_back(evt->getData().getInteger());			
			++numEventsRead;
		}
	}
	
	bmt_assert("there are events left", !mbr->nextEventExists());
	
	for(int i = 0; i<bmt_NUM_THREADS; ++i) {
		int currValue = -1;
		for(std::vector<int>::iterator j=eventCodeReceived[i].begin();
			j!=eventCodeReceived[i].end();
			++j) {
			
			bmt_assert("incorrect data matchup", currValue+1 == *j);
			currValue = *j;
		}	
	}	
	
	boost::mutex::scoped_lock lock(terLock);
	totalEventsRead += numEventsRead;
	return 0;
}

*/

