#include <iostream>
#include "Source/ThreadPool.h"

//test functions
void TestFunc1()
{
	std::cout << "TestFunc1 is working" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::cout << "TestFunc1 finished working" << std::endl;
}

void TestFunc2()
{
	std::cout << "TestFunc2 is working" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(2));
	std::cout << "TestFunc2 finished working" << std::endl;
}

void TestFunc3()
{
	std::cout << "TestFunc3 is working" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(3));
	std::cout << "TestFunc3 finished working" << std::endl;
}

void TestFunc4()
{
	std::cout << "TestFunc4 is working" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(4));
	std::cout << "TestFunc4 finished working" << std::endl;
}

void TestFunc5()
{
	std::cout << "TestFunc5 is working" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "TestFunc5 finished working" << std::endl;
}

void TestFunc(int i)
{
	std::cout << "TestFunc " << i << " is working" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(i));
	std::cout << "TestFunc " << i << " finished working" << std::endl;
}

int main()
{
	//a series of testst for the ThreadPool functionality
	
	std::function<void()> f1 = TestFunc1;
	std::function<void()> f2 = TestFunc2;
	std::function<void()> f3 = TestFunc3;
	std::function<void()> f4 = TestFunc4;
	std::function<void()> f5 = TestFunc5;
	std::function<void()> f6 = std::bind(TestFunc, 6); //a way to pass a function with variable amount of arguements to ThreadPool manager
	
	ThreadPool ThreadPoolManager(4);

	std::thread MainPoolManagerThread(&ThreadPool::LaunchThreadPool, &ThreadPoolManager);
	MainPoolManagerThread.detach();

	uint64_t Task3ID = ThreadPoolManager.AddTask(f3);
	uint64_t Task5ID = ThreadPoolManager.AddTask(f5);

	ThreadPoolManager.wait(Task5ID);

	ThreadPoolManager.AddTask(f1);
	ThreadPoolManager.AddTask(f2);
	ThreadPoolManager.AddTask(f4);

	ThreadPoolManager.wait_all();

	ThreadPoolManager.AddTask(f6);
	std::cout << "Adding TestFunc(int i) task after wait_all(). Should happen after tasks 1, 2 and 4 are completed" << std::endl;

	char a;
	std::cin >> a;

	return 0;
}
