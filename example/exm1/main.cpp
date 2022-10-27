#include <cstdio>
#include <thread>
#include <unistd.h>
#include <sys/time.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

using namespace std;

#include "log/log.h"

int main(int argc, char *argv[])
{
	std::thread* thd[100] = { 0 }; 
	bool running = true;

	log_init(LOG_MODE_FILE, LOG_LEVEL_DEBUG, "./", "exm1");


	size_t len = 10240L * 4096;
	char *buff =  (char*)calloc(len, 1);
	thd[99] = new std::thread([&](){
		while (0 && running)
		{
			struct timeval tv;
			gettimeofday(&tv, NULL);
			char filename[100] = { 0 };
			sprintf(filename, "%ld.raw", tv.tv_sec * 1000 + tv.tv_usec / 1000);
			FILE *file = fopen(filename, "w");
			fwrite(buff, 1, len, file);
			fclose(file);
			usleep(6000);
	  }
	});

	for (int i = 0; i < 1; ++i)
	{
		thd[i] = new std::thread([&](int n){
			struct timeval tv1, tv2;
			long long maxDelay = 0;
			while (running)
			{
				gettimeofday(&tv1, NULL);
				LOG_DEBUG("AAAAAAAAAAAAAAAAAAAAAAAAAAA: %d", n);
				gettimeofday(&tv2, NULL);
				long long t = tv2.tv_sec * 1000 * 1000 + tv2.tv_usec - tv1.tv_sec * 1000 * 1000 - tv1.tv_usec;
				
				if (t >= maxDelay)
				{
					maxDelay = t;
					printf("%ld diffence %lld us\n", tv2.tv_sec * 1000 + tv2.tv_usec / 1000, t);
				}
				usleep(1000);
			}
		}, i);
	}

	for (int i = 0; i < 3; ++i)
	{
		thd[i]->join();
	}
	return 0;
}
