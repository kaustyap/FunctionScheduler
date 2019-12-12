#include <iostream>
#include <chrono>
#include <functional>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

using namespace std::chrono_literals;

class FunctionScheduler
{
	using system_time_point = std::chrono::time_point<std::chrono::system_clock>;

	public:

		FunctionScheduler() = default;
		FunctionScheduler(const FunctionScheduler&) = delete;
		FunctionScheduler& operator=(const FunctionScheduler&) = delete;

		template <typename timeType>
		void run_after(timeType time, std::function<void()> job)
		{
			std::scoped_lock<std::mutex> lock(m_Mutex);
			// Schedule each job in future time
			m_Jobs.insert({std::chrono::system_clock::now() + time, job});
			m_Condition.notify_all();
		}

		void scheduler()
		{
			while(m_IsRunning)
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				// Wait until we get notification of any job arrival. The predicate is necessary to avoid lost/spurious wakeup...
				m_Condition.wait(lock, [&]{ return !m_Jobs.empty(); });

				// Get the most recent job to schedule which will be at the beginning thanks to sorted map
				system_time_point recent_job_time = m_Jobs.begin()->first;	

				// Wait until time to start the most recent job has passed...
				if (recent_job_time != std::chrono::system_clock::now() &&
						m_Condition.wait_until(lock, recent_job_time) == std::cv_status::timeout)
				{
					// Iterate all jobs (functions) scheduled to run at the same time 
					auto multimap_it = m_Jobs.equal_range(recent_job_time);
					for (auto func_it = multimap_it.first; func_it != multimap_it.second; ++func_it)
					{
						std::chrono::duration<double> diff = std::chrono::system_clock::now() - m_StartTime;
						std::cout << "Starting new job at: " << diff.count() << std::endl;
						// Execute each job in separate thread in detached mode...
						auto t = std::thread(func_it->second);
						t.detach();
					}
					m_Jobs.erase(m_Jobs.begin());
						
				}

			}

		}

		void startScheduling()
		{
			m_IsRunning = true;
			m_StartTime = std::chrono::system_clock::now();
			m_Scheduler = std::thread(&FunctionScheduler::scheduler, this);	
			m_Scheduler.join();
		}
	
		void stopScheduling()
		{
			m_IsRunning = false;
		}

		~FunctionScheduler()
		{
			stopScheduling();
			if (m_Scheduler.joinable())
			{
				m_Scheduler.join();
			}
		}

	private:
		std::atomic<bool> m_IsRunning {false};
		std::multimap<system_time_point, std::function<void()> > m_Jobs; 	
		std::thread m_Scheduler;
		std::mutex m_Mutex;
		std::condition_variable m_Condition;
		system_time_point m_StartTime;

};

void reg(FunctionScheduler& f)
{
	std::this_thread::sleep_for(5s);
	f.run_after(5s, []() { std::cout << "Hello51!" << std::endl; std::this_thread::sleep_for(10s); });
	f.run_after(5s, []() { std::cout << "Hello52!" << std::endl; std::this_thread::sleep_for(30s); });
	std::this_thread::sleep_for(100s);
}

int main()
{
	FunctionScheduler f;
	std::thread t(reg, std::ref(f));

	f.run_after(15s, [](){ std::cout << "Hello15!" << std::endl; });

	f.startScheduling();
	return 0;
}

