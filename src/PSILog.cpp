// PSILog.cpp
//
// Class for handling logging, with support for multiple outputs, thread safety and modular extensions of
// user configurable output destinations
//
// Copyright (c) 2018 Sakari Lehtonen <sakari AT psitriangle DOT net>

#include <assert.h>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <thread>
#include <mutex>

#include "PSILog.h"

// Default logger() << "Log message" overriding
// Override the PSILog functor operator, to return a LogStream that
// references this logger. This way the the operations are thread safe, as
// this temporary LogStream stringstream will call the actual log() operation when it is destroyed
PSILogStream PSILog::operator ()() {
	return PSILogStream(*this, Level::MSG);
}

// Same for one where the user specifies the log level
// eg. logger(PSILog::ERR) << "Log message" overriding
PSILogStream PSILog::operator ()(int log_level) {
	return PSILogStream(*this, log_level);
}

// Destructor for PSILogStream. This is when the actual logging happens.
PSILogStream::~PSILogStream() {
	// Filter log messages with the binary arithmetic mask
	/*
	std::cout << "_log.get_filter() = " << _log.get_filter()
		  << " _log_level =  " << _log_level << std::endl;
	*/
	if (_log.get_filter() & _log_level) {
		_log.log(str(), _log_level);
	}
}

// Apply formatting and dispatch the log message to all of our outputs
void PSILog::log(const std::string &entry, int log_level) {
	std::stringstream entry_ss;

	// Insert the prefix in the beginning of the entry
	if (get_add_prefix() == true) {
		std::string prefix = get_log_entry_prefix(entry);
		entry_ss << prefix << entry;
	} else {
		entry_ss << entry;
	}

	// Add default output if we don't have any outputters
	if (_outputs.size() == 0) {
		add_output(make_unique<PSILogConsoleOutput>());
	}

	// Write to all of our outputs
	for (const auto &outputter : _outputs) {
		outputter->write_log_entry(entry_ss.str(), log_level);
	}
}

// Get the default log entry prefix, return a timestamp for now
// TODO: provide a way for the user to override this method, to implement custom
// prefixes easily
std::string PSILog::get_log_entry_prefix(const std::string &log_entry) const {
	std::string formatted;
	std::stringstream ss;

	// Append time and current thread id to the entry
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);

	// Add timestamp
	ss << std::put_time(&tm, "[%H:%M:%S] ");

	if (_add_thread_id == true) {
		std::thread::id thread_id = std::this_thread::get_id();
		ss << "[" << thread_id << "] ";
	}

	return ss.str();
}

// Message all of our outputters to flush their output
void PSILog::flush() {
	for (const auto &outputter : _outputs) {
		outputter->flush();
	}
}

// Add output destination to our chain of outputs
void PSILog::add_output(std::unique_ptr<PSILogOutput> output) {
	assert(output != nullptr);
	_outputs.push_back(std::move(output));
}

// Default console output implementation
// Write the the log entry to console
bool PSILogConsoleOutput::write_log_entry(const std::string &log_entry, int log_level) {
	// Log errors to stderr
	if (log_level == PSILog::ERR) {
		std::cerr << log_entry;
		std::cerr.flush();
	} else {
		std::cout << log_entry;
		std::cout.flush();
	}

	return true;
}

void PSILogConsoleOutput::flush() {
	std::cerr.flush();
	std::cout.flush();
}

// Default file output implementation
PSILogFileOutput::PSILogFileOutput(const char *output_path) {
	//fprintf(stderr, "Initialized FileOutput with output_path = %s\n", output_path);
	_output_path = output_path;

	// Open the file for appending at the end of the log file
	_fs.open(output_path, std::fstream::out | std::fstream::app);
}

PSILogFileOutput::~PSILogFileOutput() {
	_fs.close();
}

void PSILogFileOutput::flush() {
	_fs.flush();
}

// Write the the log entry to our file
bool PSILogFileOutput::write_log_entry(const std::string &log_entry, int log_level) {
	// The operations should already be thread safe, but let's just make sure
	// file operations are guarded behind a mutex
	std::lock_guard<std::mutex> guard(_mutex);

	_fs << log_entry;
	_fs.flush();

	return true;
}
