#pragma once

#include <functional>
#include <mutex>
#include <future>
#include <map>
#include <set>
#include <queue>
#include <mutex>

//wrapper class for our tasks
class AsyncTask
{
private:
	
	AsyncTask() = delete;

	uint64_t TaskID;
	
	std::once_flag callflag;

	std::function<class T()>& TaskToRun;

public:

	AsyncTask(std::function<class T()>& _InTask, uint64_t _ThisTaskID) : TaskToRun(_InTask), TaskID(_ThisTaskID) {	};

	uint64_t GetTaskID() const { return TaskID; }

	//starts asyncronous task
	void StartTask(); 

	void Wait(); //can only be called once

};

//main class managering threads
//TODO: consider making it singleton
class ThreadPool
{
private:

	//Mutextes and condition variables:
	std::mutex M_MainFunction;

	std::mutex M_TasksQueue;

	std::mutex M_ThreadsStatus;

	std::condition_variable CV_MainFunction;

	//Other variables:
	bool bMainThreadUnlocked = true;

	size_t MaxThreads = 0;

	//Data arrays:

	//Vector with all task threads
	//Possibly not needed
	std::vector<std::thread> ActiveThreads;

	//Map containing all worker threads' statuses. One record per each Task Thread(aka ThreadWorker()); 
	//in std::pair, bool indicates thread's status(working/not working), uint64_t - active task's ID
	std::map<std::thread::id, std::pair<bool, uint64_t>> ThreadsStatus; 

	//Tasks' queue
	std::queue<AsyncTask> TasksQueue; 

	//Tasks' unique IDs'
	std::set<uint64_t> TaskIDs; //assures unique TaskIDs

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
	//Does not accept functions with variable amount of random arguements at the moment
	uint64_t AddTask(std::function<class T()>& _InFunc);
};