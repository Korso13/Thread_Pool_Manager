#pragma once

#include <functional>
#include <mutex>
#include <shared_mutex>
#include <future>
#include <map>
#include <set>
#include <deque>
#include <mutex>

//wrapper class for our tasks
class AsyncTask
{
private:
	
	AsyncTask() = delete;

	uint64_t TaskID;

	std::function<void()>& TaskToRun;

	//std::condition_variable CV;

	bool bNotifyMainThreadOnFinish = false;

public:

	AsyncTask(std::function<void()>& _InTask, uint64_t _ThisTaskID) : TaskToRun(_InTask), TaskID(_ThisTaskID) {	};

	~AsyncTask();

	uint64_t GetTaskID() const { return TaskID; }

	//starts asyncronous task
	void StartTask() {TaskToRun();};

	//const std::condition_variable& GetTaskCV() const { return CV; }

	void SetNotifyMainThreadOnFinish() { bNotifyMainThreadOnFinish = true; }

	bool ShouldNotifyMainThreadOnFinish() const { return bNotifyMainThreadOnFinish; }
};

//main class managering threads
//TODO: consider making it singleton
class ThreadPool
{
private:

	//Mutextes and condition variables:
	std::mutex M_MainFunction;

	std::shared_mutex M_TasksQueue;

	std::shared_mutex M_ThreadsStatus;

	std::shared_mutex M_TaskIDs;

	std::condition_variable CV_MainFunction;

	//Other variables:
	bool bMainThreadUnlocked = true;

	size_t MaxThreads = 0;

	//Data arrays:

	//Map containing all worker threads' statuses. One record per each Task Thread(aka ThreadWorker()); 
	//in std::pair, bool indicates thread's status(working/not working), uint64_t - active task's ID
	std::map<std::thread::id, std::tuple<bool, uint64_t, AsyncTask*>> ThreadsStatus;

	//Tasks' queue
	std::deque<AsyncTask*> TasksQueue; 

	//Tasks' unique IDs'
	std::map<uint64_t, std::thread::id> TaskIDs; //assures unique TaskIDs

private:

	//deleted default constructor
	ThreadPool() = delete;

	//predicate for use in Wait() and Wait_all() methods; passed to condition variable and called, when it fires a notice
	bool IsMainThreadUnlocked() { return bMainThreadUnlocked; };

	//launches class main thread upon construction; Initializes all Task Threads(aka ThreadWorker())
	void LaunchThreadPool();

	//Task Threads' function working in infinite loop
	void ThreadWorker();

public:
	
	ThreadPool(size_t _MaxThreads);

	//adds a new function to the pool of tasks
	//Does not accept functions with variable amount of random arguements or returns other than void at the moment
	uint64_t AddTask(std::function<void()>& _InFunc);

	void wait(uint64_t _TaskID);

	void wait_all();
};
