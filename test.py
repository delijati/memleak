import signal
import os
import traceback
import memerr


def debug_signal_handler(signal, frame):
    traceback.print_stack(frame)


def main():
    # print(memerr.random_noise(1000))
    print("PID: %s" % os.getpid())
    for i in range(100000):
        memerr.random_noise(i, False)


if __name__ == "__main__":
    signal.signal(signal.SIGUSR1, debug_signal_handler)
    main()
