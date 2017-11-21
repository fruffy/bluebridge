# playing with multiprocessing in Python.
from multiprocessing import Pool
import pymp

def simple_func(x):
	return x*x + x

if __name__ == '__main__':
	p = Pool(5)
	print p.map(simple_func, [1,2,3,4,5])	

