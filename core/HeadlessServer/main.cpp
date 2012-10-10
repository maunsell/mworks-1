/**
 * main.cpp
 *
 * Description: A simple headless server application.
 *
 * History:
 * David Cox on 6/13/05 - Created.
 * 
 * Copyright 2005 MIT. All rights reserved.
 */

#include "MWorksCore/TestBedCoreBuilder.h"

#include "MWorksCore/Utilities.h"
#include "MWorksCore/Experiment.h"

#include "MWorksCore/Scheduler.h"
#include "MWorksCore/StateSystem.h"
#include "MWorksCore/Event.h"
#include "MWorksCore/PluginServices.h"
#include "MWorksCore/OpenGLContextManager.h"
#include "MWorksCore/FakeMonkey.h"
#include "MWorksCore/LoadingUtilities.h"
#include "MWorksCore/ITC18_IODevice.h"
#include "MWorksCore/StandardVariables.h"
#include "MWorksCore/Timer.h"
#include "MWorksCore/ScarabServer.h"

#include "MWorksCore/FilterTransforms.h"

#include "MWorksCore/CoreBuilderForeman.h"
#include "MWorksCore/StandardServerCoreBuilder.h"
#include "MWorksCore/DefaultServer.h"

#include <iostream>
#include <pthread.h>
#include <time.h>
using namespace mw;




int main(int argc, char *argv[]) {

	#ifdef	USE_GLUT_OPENGL
		// graphics will be done with GLUT
		glutInit(&argc, argv);
	#endif


	try {
		
		// Build the core
		CoreBuilderForeman::constructCoreStandardOrder(
											new StandardServerCoreBuilder());
		DefaultServer *server = new DefaultServer();
		
		ScarabServer *scarab_server = server->getScarabServer();
		scarab_server->setServerHostname("127.0.0.1");
		server->startServer();
		server->startAccepting();
		
	} catch (...) {
		printf("Caught exception.");
	}


	const struct timespec display_update_timeout = {(time_t)0, 5000000L};

	bool done = false;
	
	while(!done){
	
		// because of OpenGL thread unsafety, the state system needs to run
		// directly in this thread.  Thus, we'll block on this pthread condition
		// until the event handler tells us to go
		pthread_cond_wait(&state_system_start_cond, &state_system_start_mutex);
		
		// we need to make sure that the OpenGL contexts are finalized
		// (initialized) in this thread due to thread safety issues with
		// both GLUT and SDL (and OpenGL in general on Linux)
		shared_ptr<OpenGLContextManager> opengl_context_manager = OpenGLContextManager::instance();
        opengl_context_manager->finalizeContexts();
				
		shared_ptr <StateSystem> state_system = StateSystem::instance();
		state_system->start();
		
		
		// block until we get a broadcast that the display needs updating
		// or a specific time interval elapses
		int returnval = 
			pthread_cond_timedwait(&display_update_cond, 
								   &display_update_mutex,
								   &display_update_timeout);
		
		// if the condition wait didn't time-out, then update the display
		// Ultimately, we might prefer a queue of displays to update here
		// if, say, we had more than one display in an experiment
		if(returnval = !ETIMEDOUT){
			if(GlobalCurrentExperiment != NULL){
				GlobalCurrentExperiment->updateDisplay();
			}
		}
		
		// Process other events
		#ifdef  USE_SDL_OPENGL
			// Get the state of the keyboard keys
    			unsigned char *keys = SDL_GetKeyState(NULL);

    			// and check if ESCAPE has been pressed. If so then quit
    			if(keys[SDLK_ESCAPE]) done=1;
		#endif
		
		#ifdef	USE_GLUT_OPENGL
			glutMainLoop();
		#endif
		
		//clock->sleepMS(100);
	}

}


void *SDL

