#pragma once
#include <deque>
#include <thread>
#include <mutex>
#include <chrono>
#include <functional>

#include "ZLC.hpp"

template<class Compressor>
class MultithreadCompressor
{
public:
	enum class Mode { compress, decompress };

	typedef std::pair<std::string, std::vector<uint8_t>> task_t;
	typedef std::deque<task_t> queue_t;

	typedef std::function<void(const task_t&)> task_started_callback_t;
	typedef std::function<void(const task_t&)> task_finished_callback_t;

private:
	enum class State
	{
		idle,
		compressing,
		decompressing
	};

	const int _thread_count;
	State _state = State::idle;
	
	queue_t _inputs;
	queue_t _outputs;

	std::mutex _in_mutex;
	std::mutex _out_mutex;

	size_t _mem_usage;

	std::vector<std::thread> _threads;
	std::vector<bool> _threads_busy;
	bool _should_stop = false;

	task_started_callback_t _task_started_callback;
	task_finished_callback_t _task_finished_callback;

public:
	MultithreadCompressor(int threads = 0) :
		_thread_count( // threads ? threads : (std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4))
			[](int tc) {
				if (tc == 0)
				{
					tc = std::thread::hardware_concurrency();
					if (tc == 0)
						tc = 4;
				}
				return tc;
			}(threads)),
		_mem_usage(0),
		_threads(_thread_count),
		_threads_busy(_thread_count, false)
	{
	}

	~MultithreadCompressor()
	{
		try
		{
			if (_state != State::idle)
			{
				_should_stop = true;
				for (auto& t : _threads)
					t.join();
			}
		}
		catch (...)
		{
			std::cerr << "Unexpected error during termination of compression threads!\n";
		}
	}
	

	void task_started_callack(const task_started_callback_t& callback)
	{
		_task_started_callback = callback;
	}

	void task_finished_callback(const task_finished_callback_t& callback)
	{
		_task_finished_callback = callback;
	}

	void push(const task_t& task)
	{
		_in_mutex.lock();
		_inputs.push_back(task);
		_in_mutex.unlock();
		_mem_usage += task.second.size();
	}

	void emplace(task_t&& task)
	{
		_in_mutex.lock();
		task_t& t = _inputs.emplace_back(task);
		_in_mutex.unlock();
		_mem_usage += t.second.size();
	}

	bool try_pop(task_t& result)
	{
		_out_mutex.lock();
		if (_outputs.empty())
		{
			_out_mutex.unlock();
			return false;
		}
		result = std::move(_outputs.front());
		_outputs.pop_front();
		_out_mutex.unlock();
		_mem_usage -= result.second.size();
		return true;
	}

	task_t pop()
	{
		task_t result;
		bool succ;
		while (true)
		{
			succ = try_pop(result);
			if (succ)
				return std::move(result);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void start(Mode mode)
	{
		if (_state != State::idle)
			throw std::exception("Compressor is already busy!");
		_should_stop = false;
		_state = mode == Mode::compress ? State::compressing : State::decompressing;
		for (int i = 0; i < _thread_count; i++)
			_threads[i] = std::thread(&MultithreadCompressor::thread_main, this, i);
	}

	void stop() { _should_stop = true; }

	void stop_wait()
	{
		if (_state == State::idle)
			return;
		_should_stop = true;
		for (auto& t : _threads)
			t.join();
	}

	size_t memory_usage() { return _mem_usage; }

private:

	void thread_main(int tid)
	{
		size_t input_size;

		_threads_busy[tid] = true;

		while (!_should_stop)
		{
			_in_mutex.lock();
			if (_inputs.empty())
			{
				_in_mutex.unlock();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
			task_t task = std::move(_inputs.front());
			_inputs.pop_front();
			_in_mutex.unlock();
			
			input_size = task.second.size();
			
			if (_task_started_callback)
				_task_started_callback(task);
			if (_state == State::compressing)
				task.second = std::move(Compressor::template compress<ZlcDict>(task.second));
			else task.second = std::move(Compressor::decompress(task.second));
			
			_mem_usage -= input_size;
			_mem_usage += task.second.size();
			if (_task_finished_callback)
				_task_finished_callback(task);

			_out_mutex.lock();
			_outputs.emplace_back(std::move(task));
			_out_mutex.unlock();
		}

		_threads_busy[tid] = false;

		// the last one out turns off the light
		bool finished = true;
		for (bool b : _threads_busy)
			if (b) finished = false;
		if (finished)
			_state = State::idle;
	}
};