#include "TokenWorker.h"
#include <thread>
#include "Helpers/ReqRunner.h"
#include <iostream>
#include <chrono>
void TokenWorker() {

	while (true) {
		using namespace std::chrono_literals; 
		ReqRunner::GetToken();
		std::this_thread::sleep_for(8min);
	}
}