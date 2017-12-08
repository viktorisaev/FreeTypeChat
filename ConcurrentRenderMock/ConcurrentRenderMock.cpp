// ConcurrentRenderMock.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

bool g_Quit = false;

std::mutex g_PresentMutex;


std::mutex g_InputQueueMutex;
std::queue<char, std::deque<char>> g_InputQueue;		// typed characters to be processed

std::mutex g_FreeTypeCacheMutex;
std::vector<char> g_FreeTypeCacheVector;				// current characters cache (all used characters)

std::mutex g_FreeTypeQueueMutex;
std::condition_variable g_FreeTypeQueueConditionalVariable; 
std::queue<char, std::deque<char>> g_FreeTypeQueue;		// current characters to be chached

std::mutex g_OutputMutex;
std::vector<char> g_OutputVector;						// output characters



void PresentStateInput()
{
	std::lock_guard<std::mutex> locker_present(g_PresentMutex);	// auto-unlock on block quit
	std::lock_guard<std::mutex> locker_inputqueue(g_InputQueueMutex);	// auto-unlock on block quit
	std::lock_guard<std::mutex> locker_output(g_OutputMutex);	// auto-unlock on block quit

	system("cls");


	printf("\n");
	printf("\n");

	printf("\n");
	printf("\n");

	printf("Input queue (%i):\n", g_InputQueue.size());
	for (auto it = g_InputQueue._Get_container().begin(); it != g_InputQueue._Get_container().end(); ++it)
	{
		printf("%c", *it);
	}
	printf("\n");

	printf("Output (%i):\n", g_OutputVector.size());
	for (auto it = g_OutputVector.begin(); it != g_OutputVector.end(); ++it)
	{
		printf("%c", *it);
	}
	printf("\n");
}



void PresentStateFreeType()
{
	std::lock_guard<std::mutex> locker_present(g_PresentMutex);	// auto-unlock on block quit
//	std::lock_guard<std::mutex> locker_freetypequeue(g_FreeTypeQueueMutex);	// auto-unlock on block quit
	std::lock_guard<std::mutex> locker_freetypecache(g_FreeTypeCacheMutex);	// auto-unlock on block quit

	system("cls");


	printf("FreeType queue (%i):\n", g_FreeTypeQueue.size());
	for (auto it = g_FreeTypeQueue._Get_container().begin(); it != g_FreeTypeQueue._Get_container().end(); ++it)
	{
		printf(" %c", *it);
	}
	printf("\n");

	printf("FreeType cache (%i):\n", g_FreeTypeCacheVector.size());
	for (auto it = g_FreeTypeCacheVector.begin(); it != g_FreeTypeCacheVector.end(); ++it)
	{
		printf(" %c", *it);
	}
	printf("\n");
}




Concurrency::task<void> CreateInputTask()
{
	return Concurrency::create_task([]
	{
		while (!g_Quit)
		{
			char c = _getch();

			if (c == 27)
			{
				g_Quit = true;
				g_FreeTypeQueueConditionalVariable.notify_all();
			}
			else
			{
				{
					std::lock_guard<std::mutex> locker_inputqueue(g_InputQueueMutex);	// auto-unlock on block quit
					g_InputQueue.push(c);
				}

				bool found = false;
				{
					std::lock_guard<std::mutex> locker_freetypecache(g_FreeTypeCacheMutex);	// auto-unlock on block quit
					std::vector<char>::iterator itUpperBound = std::lower_bound(g_FreeTypeCacheVector.begin(), g_FreeTypeCacheVector.end(), c);

					if (itUpperBound != g_FreeTypeCacheVector.end() && *itUpperBound == c)
					{
						// found in cache! to be processed to output
						found = true;
					}
				}

				if (!found)
				{
					// not in the cache, wait for to be cached
					{
						std::lock_guard<std::mutex> locker_freetypequeue(g_FreeTypeQueueMutex);	// auto-unlock on block quit
						g_FreeTypeQueue.push(c);
					}
					g_FreeTypeQueueConditionalVariable.notify_all();
				}

				PresentStateInput();
			}

		}
	});
}



Concurrency::task<void> CreateFreeTypeTask()
{
	return Concurrency::create_task([]
	{
		while (!g_Quit)
		{
			std::unique_lock<std::mutex> lk(g_FreeTypeQueueMutex);
			g_FreeTypeQueueConditionalVariable.wait(lk, [] { return g_Quit || !g_FreeTypeQueue.empty(); });
			lk.unlock();

			while (g_FreeTypeQueue.size() > 0)
			{
				char c;
				{
//					std::lock_guard<std::mutex> locker_freetypequeue(g_FreeTypeQueueMutex);
					c = g_FreeTypeQueue.front();
					g_FreeTypeQueue.pop();
				}

				PresentStateFreeType();

				bool needUpdate = false;
				{
					{
						std::lock_guard<std::mutex> locker_freetypecache(g_FreeTypeCacheMutex);	// auto-unlock on block quit
						std::vector<char>::iterator itUpperBound = std::lower_bound(g_FreeTypeCacheVector.begin(), g_FreeTypeCacheVector.end(), c);

						if (itUpperBound == g_FreeTypeCacheVector.end() || *itUpperBound != c)
						{
							g_FreeTypeCacheVector.insert(itUpperBound, c);
							needUpdate = true;
						}
					}	// release FreeTypeCache
				}

				if (needUpdate)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					PresentStateFreeType();
				}
			}
		}
	});
}



Concurrency::task<void> CreateOutputTask()
{
	return Concurrency::create_task([]
	{
		while (!g_Quit)
		{
			if (g_InputQueue.size() > 0)
			{
				char c;
				bool needUpdate = false;
				{
					std::lock_guard<std::mutex> locker_inputqueue(g_InputQueueMutex);	// auto-unlock on block quit
					c = g_InputQueue.front();		// safe to pick the value (???)

					if (c == 8)
					{
						// backspace
						g_InputQueue.pop();

						std::lock_guard<std::mutex> locker_output(g_OutputMutex);	// auto-unlock on block quit
						g_OutputVector.pop_back();

						needUpdate = true;
					} // release Output
					else
					{
						bool found = false;
						{
							std::lock_guard<std::mutex> locker_freetypecache(g_FreeTypeCacheMutex);	// auto-unlock on block quit
							std::vector<char>::iterator itUpperBound = std::lower_bound(g_FreeTypeCacheVector.begin(), g_FreeTypeCacheVector.end(), c);

							if (itUpperBound != g_FreeTypeCacheVector.end() && *itUpperBound == c)
							{
								found = true;
							}
							// else - skip the iteration and wait for the character to appear in the cache
						}	// release FreeTypeCache

						if (found)
						{
							g_InputQueue.pop();

							std::lock_guard<std::mutex> locker_output(g_OutputMutex);	// auto-unlock on block quit
							g_OutputVector.push_back(c);

							needUpdate = true;
						} // release Output
					}
				}	// release InputQueue

				if (needUpdate)
				{
					PresentStateInput();
				}
			}
		}
	});
}



int main()
{
	PresentStateInput();
	PresentStateFreeType();

	auto inputTask = CreateInputTask();
	auto freeTypeTask = CreateFreeTypeTask();
	auto outputTask = CreateOutputTask();

	(inputTask && freeTypeTask && outputTask).get();


    return 0;
}

