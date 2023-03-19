#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t _MaxThreads)
{
	MaxThreads = _MaxThreads;

	LaunchThreadPool();
}

void ThreadPool::LaunchThreadPool()
{
	//reserve the 0 TaskID for state of no task performed
	TaskIDs.insert(std::pair < uint64_t, std::thread::id>(0, std::this_thread::get_id()));
	
	//create a pool of worker threads that will be pulling tasks from TasksQueue
	for (size_t i = 0; i < MaxThreads; i++)
	{
		std::thread NewThread(&ThreadPool::ThreadWorker, this);
		ThreadsStatus.emplace(NewThread.get_id(), std::make_tuple(false, 0, nullptr));
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
	std::shared_lock<std::shared_mutex> TaskIDLock(M_TaskIDs);
	
	uint64_t CurrentTaskID;
	
	//ThreadWorker work loop - looks for jobs in TasksQueue and if finds some, grabs and does it
	while (true)
	{
		QueueLock.lock();

		if (TasksQueue.size() > 0)
		{
			CurrentTaskID = TasksQueue.front()->GetTaskID();
			AsyncTask* CurrentTask = TasksQueue.front();
			TasksQueue.pop_front(); //remove the task from queue
			QueueLock.unlock();

			ThreadStatusLock.lock();
			auto It = ThreadsStatus.find(std::this_thread::get_id()); //find this thread's record in the ThreadsStatus std::map
			It->second = std::make_tuple(true, CurrentTaskID, CurrentTask); //This thread is working flag + Thread task's ID + ptr to the task's wrapper
			ThreadStatusLock.unlock();

			TaskIDLock.lock();
			TaskIDs.find(CurrentTaskID)->second = std::this_thread::get_id(); //assigning this thread's id to the TaskID (needed for wait() member function to work)
			TaskIDLock.unlock();

			if (!CurrentTask)
			{
				continue;
			}

			CurrentTask->StartTask(); //loop is stuck until the task is complete
			if (CurrentTask->ShouldNotifyMainThreadOnFinish()) //unlocks main thread if wait() was called on this task
			{
				bMainThreadUnlocked = true;
				CV_MainFunction.notify_all();
			}
			CurrentTask->~AsyncTask(); //free up teh memory allocated to keep the task wrapper
		}
		else
		{
			QueueLock.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		ThreadStatusLock.lock();
		auto It = ThreadsStatus.find(std::this_thread::get_id());
		It->second = std::make_tuple(false, 0, nullptr); //This thread is not working anymore
		ThreadStatusLock.unlock();
	}
}

uint64_t ThreadPool::AddTask(std::function<void()>& _InFunc)
{
	//trying to lock main thread (redudancy in case of wait() member functions' use)
	std::lock_guard<std::mutex> MainThreadLock(M_MainFunction);
	//assign ID to the task
	uint64_t NewTaskID = TaskIDs.size();
	TaskIDs.insert(std::pair< uint64_t, std::thread::id>(NewTaskID, std::this_thread::get_id())); //thread_id is temporary - fill be replaced once the task is launched by a specific task thread

	//create wrapper for the task
	AsyncTask* NewTask = new AsyncTask(_InFunc, NewTaskID);

	//throw it into queue, where the first free ThreadWorker will grab it
	if (NewTask)
	{
		TasksQueue.push_back(NewTask);
		return NewTaskID;	
	}
	
	return 0;
}

void ThreadPool::wait(uint64_t _TaskID)
{
	std::shared_lock<std::shared_mutex> QueueLock(M_TasksQueue);
	std::shared_lock<std::shared_mutex> ThreadStatusLock(M_ThreadsStatus);
	std::shared_lock<std::shared_mutex> TaskIDLock(M_TaskIDs);
	
	TaskIDLock.lock();
	auto It = TaskIDs.find(_TaskID);
	if (It == TaskIDs.end())
	{
		return; //no such task was ever created
	}

	std::thread::id TaskThreadID;

	if (It->second != std::this_thread::get_id()) //means that the task might be worked on in the thread
	{
		TaskThreadID = It->second;
		TaskIDLock.unlock();
		ThreadStatusLock.lock();
		auto TaskThreadRef = ThreadsStatus.find(TaskThreadID);
		if (TaskThreadRef != ThreadsStatus.end())//the task IS still being worked on
		{
			AsyncTask* FoundTask = std::get<2>(TaskThreadRef->second);
			if (!FoundTask)
			{
				return;
			}
			
			//we tell the task's worker thread to unlock teh main thread once the task is done
			FoundTask->SetNotifyMainThreadOnFinish();
			ThreadStatusLock.unlock();
			std::lock_guard<std::mutex> MainThreadLock(M_MainFunction); //locking main thread
			bMainThreadUnlocked = false; 
			CV_MainFunction.notify_all();
			return;
		}
		else 
		{
			ThreadStatusLock.unlock(); //task has already been completed
			return;
		}
	}
	else //means that the task is in the queue
	{
		TaskIDLock.unlock();
		QueueLock.lock();
		for (const auto Task : TasksQueue)
		{
			if (!Task)
				continue;

			if (Task->GetTaskID() == _TaskID)
			{
				Task->SetNotifyMainThreadOnFinish(); //we tell the task's worker thread to unlock teh main thread once the task is done
				break;
			}
		}
		QueueLock.unlock();
		std::lock_guard<std::mutex> MainThreadLock(M_MainFunction);//locking main thread
		bMainThreadUnlocked = false; 
		CV_MainFunction.notify_all();
		return;
	}
}

void ThreadPool::wait_all()
{
	bMainThreadUnlocked = false; //locking main thread
	std::lock_guard<std::mutex> MainThreadLock(M_MainFunction);

	auto WatcherLambda = [this](std::lock_guard<std::mutex>* lock) {
		while (true)
		{
			bool bNoActiveTasks = true;
			for (const auto Task : ThreadsStatus)
			{
				if (std::get<0>(Task.second))
				{
					bNoActiveTasks = false;
					break;
				}
			}
			if (TasksQueue.size() == 0 && bNoActiveTasks)
			{
				bMainThreadUnlocked = true; //unlocking main thread
				if (lock)
				{
					lock->~lock_guard();
				}
				CV_MainFunction.notify_all();
				break;
			}
		};
	};
	
	std::thread WatcherThread(WatcherLambda, &MainThreadLock); //launch the above loop in a separate thread to avoid freezing it along with the main one
	WatcherThread.detach();

	CV_MainFunction.notify_all(); //locks the main thread until a detached watcher thread unlocks it
	return;
}