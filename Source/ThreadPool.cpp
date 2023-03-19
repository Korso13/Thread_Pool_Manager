#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t _MaxThreads)
{
	MaxThreads = _MaxThreads;

	LaunchThreadPool();
}

void ThreadPool::LaunchThreadPool()
{
	//reserve the 0 TaskID for state of no task performed
	TaskIDs.insert(std::pair < uint64_t, AsyncTask*>(0,nullptr));
	
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
	uint64_t CurrentTaskID;
	std::condition_variable_any CV;

	//ThreadWorker work loop - looks for jobs in TasksQueue and if finds some, grabs and does it
	while (true)
	{
		if (TasksQueue.size() > 0) //there are available tasks
		{
			AsyncTask* CurrentTask;
			{
				std::lock_guard<std::recursive_mutex> ThreadsLock(M_ThreadsStatus);
				CurrentTaskID = TasksQueue.front()->GetTaskID();
				CurrentTask = TasksQueue.front();
				TasksQueue.pop_front(); //remove the task from queue
			}

			{
				std::lock_guard<std::recursive_mutex> TasksLock(M_TasksQueue);
				auto It = ThreadsStatus.find(std::this_thread::get_id()); //find this thread's record in the ThreadsStatus std::map
				It->second = std::make_pair(true, CurrentTaskID); //This thread is working flag + Thread task's ID + ptr to the task's wrapper
			}

			if (!CurrentTask)
			{
				continue;
			}

			CurrentTask->StartTask(); //loop is stuck until the task is complete

			if (CurrentTask->ShouldNotifyMainThreadOnFinish()) //unlocks main thread if wait() was called on this task
			{
				NumberOfLockingTasks.store(NumberOfLockingTasks.load() - 1); 
				if (NumberOfLockingTasks.load() == 0)
				{
					bMainThreadUnlocked = true;
					CV_MainFunction.notify_all();
				}
			}
			CurrentTask->~AsyncTask(); //free up the memory allocated to keep the task wrapper

			//Mark this thread as not working anymore
			{
				std::lock_guard<std::recursive_mutex> ThreadsLock(M_ThreadsStatus);
				auto It = ThreadsStatus.find(std::this_thread::get_id());
				It->second = std::make_pair(false, 0); 
			}
		}
		else //no tasks available; wait for 1 millisec before looking up again (can be safely reduced or removed at all)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

void ThreadPool::CallingThreadLocker()
{
	while (true)
	{
		std::unique_lock<std::mutex> ThreadLock(M_CallingThread);
		CV_MainFunction.wait(ThreadLock, [this]()->bool {return IsMainThreadUnlocked(); }); //checks status of bMainThreadUnlocked flag when any thread invokes notify_all() on CV_MainFunction condition variable
		
		//redundancy check
		if(IsMainThreadUnlocked())
			break;
	}
}

uint64_t ThreadPool::AddTask(std::function<void()>& _InFunc)
{
	//trying to lock main thread (redudancy in case of wait() member functions' use)
	std::lock_guard<std::mutex> MainThreadLock(M_MainFunction);
	
	//assign ID to the task
	uint64_t NewTaskID = TaskIDs.size();

	//create wrapper for the task
	AsyncTask* NewTask = new AsyncTask(_InFunc, NewTaskID);

	{
		std::lock_guard<std::recursive_mutex> TaskIDLock(M_TaskIDs);
		//store Task ID and pointer to its wrapper for future reference
		TaskIDs.insert(std::pair< uint64_t, AsyncTask*>(NewTaskID, NewTask));
	}
	
	//throw it into queue, where the first free ThreadWorker will grab it
	if (NewTask)
	{
		std::lock_guard<std::recursive_mutex> TasksQueueLock(M_TasksQueue);
		TasksQueue.push_back(NewTask);
		return NewTaskID;	
	}
	
	return 0;
}

void ThreadPool::wait(uint64_t _TaskID)
{
	{
		std::lock_guard<std::recursive_mutex> TaskIDLock(M_TaskIDs);
		auto It = TaskIDs.find(_TaskID);

		if (It == TaskIDs.end())
		{
			return; //no such task was ever created
		}

		if(!It->second)
		{
			return; //task is not valid, likely has already been completed
		}

		//we tell the task's worker thread to unlock teh main thread once the task is done
		It->second->SetNotifyMainThreadOnFinish();
	}

	NumberOfLockingTasks.store(NumberOfLockingTasks.load() + 1);

	std::lock_guard<std::mutex> MainThreadLock(M_MainFunction); //locking main thread
	bMainThreadUnlocked = false;
	CV_MainFunction.notify_all();

	return;
}

void ThreadPool::wait_all()
{
	auto LockerLambda = [this]() {
		
		std::lock_guard<std::recursive_mutex> TaskIDLock(M_TaskIDs);
		for (const auto Task : TaskIDs)
		{
			if (Task.second) //if task is still valid...
			{
				//we ask it to notify us, when it's done working
				Task.second->SetNotifyMainThreadOnFinish();
				//and start the lock of the main thread
				NumberOfLockingTasks.store(NumberOfLockingTasks.load() + 1);
				std::lock_guard<std::mutex> MainThreadLock(M_MainFunction);
				bMainThreadUnlocked = false;
			}
		}
		CV_MainFunction.notify_all();
		
	};
	
	std::thread LockerThread(LockerLambda); //launch the locker lambda in a separate thread to avoid freezing it along with the main one
	LockerThread.detach();

	return;
}