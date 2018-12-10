Memory leak
===========

Build memleax. Call memleax with the pid of your program. Find the leaking function call.

	$ git clone https://github.com/WuBingzheng/memleax.git
	$ cd memleax
	$ mkdir build && cmake ..
	$ ./memleax <PID>

Build and install example:
	
	$ virtualenv env
	$ env/bin/pip install cython
	$ env/bin/python setup.py build_ext --inplace

Build the PRELOAD lib:

	$ reset && clang++ -std=c++11 -g -Wall -fPIC -shared -o stack_signal.so stack_signal.cpp -ldl -lpthread -lunwind<Paste>

Call it and see the magic happen ;):

	$ LD_PRELOAD=./stack_signal.so env/bin/python test.py
