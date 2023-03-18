#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t _MaxThreads)
{
	MaxThreads = _MaxThreads;

	LaunchThreadPool();
}

void ThreadPool::LaunchThreadPool()
{
	//create a pool of worker threads that will be pulling tasks from TasksQueue
	for (size_t i = 0; i < MaxThreads; i++)
	{
		std::thread NewThread(&ThreadPool::ThreadWorker, this);
		ThreadsStatus.emplace(NewThread.get_id(), std::make_pair(false, 0));
		//ActiveThreads.push_back(NewThread);
		NewThread.detach(); //another ThreadWorker's work loop is launched
	}

	//as soon as wait() or wait_all() is called the bMainTHreadUnlocked switches to false and locks main func\thread until CV_MainFunction notified and passes predicate (IsMainThreadUnlocked) check
	while (true)
	{
		std::unique_lock<std::mutex> MainFuncLock(M_MainFunction);
		CV_MainFunction.wait(MainFuncLock, [this]()->bool {return IsMainThreadUnlocked(); });
	}
}

void ThreadPool::ThreadWorker()
{
	std::shared_lock<std::shared_mutex> QueueLock(M_TasksQueue);
	std::shared_lock<std::shared_mutex> ThreadStatusLock(M_ThreadsStatus);
	
	uint64_t CurrentTaskID;
	
	//ThreadWorker work loop - looks for jobs in TasksQueue and if finds some, grabs and does it
	while (true)
	{
		QueueLock.lock();

		if (TasksQueue.size() > 0)
		{
			CurrentTaskID = TasksQueue.front()->GetTaskID();
			AsyncTask* CurrentTask = TasksQueue.front();
			TasksQueue.pop(); //remove the task from queue
			QueueLock.unlock();

			ThreadStatusLock.lock();
			auto It = ThreadsStatus.find(std::this_thread::get_id()); //find this thread's record in the ThreadsStatus std::map
			It->second.first = true; //This thread is working
			It->second.second = CurrentTaskID; //Thread task's ID
			ThreadStatusLock.unlock();

			CurrentTask->StartTask(); //loop is stuck until the task is complete
		}
		else
		{
			QueueLock.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		ThreadStatusLock.lock();
		auto It = ThreadsStatus.find(std::this_thread::get_id());
		It->second.first = false; //This thread is not working anymore
		It->second.second = 0; //Thread task's ID
		ThreadStatusLock.unlock();
	}
}

uint64_t ThreadPool::AddTask(std::function<void()>& _InFunc)
{
	//assign ID to teh task
	uint64_t NewTaskID = TaskIDs.size();
	TaskIDs.insert(NewTaskID);

	//create wrapper for the task
	AsyncTask* NewTask = new AsyncTask(_InFunc, NewTaskID);

	//throw it into queue, where the first free ThreadWorker will grab it
	TasksQueue.push(NewTask);
	
	return NewTaskID;
}

void AsyncTask::StartTask()
{
	TaskToRun();
}

void AsyncTask::Wait()
{
	//std::call_once(callflag, &AsyncTask::);
}
