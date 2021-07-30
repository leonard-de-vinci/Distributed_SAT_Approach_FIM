#!/usr/local/bin/python3
# -*- coding: utf-8 -*-
# Authors: Julien MARTIN-PRIN

def load(path):
	lines = []
	with open(path, 'r') as inp:
		for i in inp:
			lines.append(i)
	return lines

def parse(data):
	parsed = []
	temp = []

	for i in data:
		try:
			int(i[0])
			for j in i.split('\n')[0].split(' '):
				if j != '':
					temp.append(int(j))
			if temp:
				parsed.append(temp)
			temp = []
		except:
			continue
	return parsed

def show(data):
	for i in data:
		print(i)

def clean(data):
	final = []
	for i in data:
		final.append(sorted(i))
	return sorted(sorted(final, key=lambda x:len(x)), key=lambda x:x[0])

def comp(sample, to_test):
	common = []
	in_to_test = []
	test = False

	for elt in to_test:
		for i, elt2 in enumerate(sample):
			if sorted(elt) == sorted(elt2):
				common.append(sorted(elt))
				sample.pop(i)
				test = True
		if not(test):
			in_to_test.append(elt)
		test = False

	return common, in_to_test

def checkdouble(common, in_to_test):
	double = []
	
	for elt in common:
		for i, elt2 in enumerate(in_to_test):
			if sorted(elt) == sorted(elt2):
				double.append(elt2)
				in_to_test.pop(i)
	
	return double


def results(common, double, in_to_test, sample):
	with open('results.txt', 'w') as out:
		out.write("Models in common:\n")
		for elt in common:
			out.write(f'{elt}\n')
		out.write("\nModels in double:\n")
		for elt in double:
			out.write(f'{elt}\n')
		out.write("\nModels in to_test not in sample:\n")
		for elt in in_to_test:
			out.write(f'{elt}\n')
		out.write("\nModels in sample not in to_test:\n")
		for elt in sample:
			out.write(f'{elt}\n')

def main():
	path_to_sample = input("path to sample file > ")
	path_to_to_test = input("path to test file > ")
	sample = parse(load(path_to_sample))
	to_test = parse(load(path_to_to_test))
	common, in_to_test = comp(sample, to_test)
	double = checkdouble(common, in_to_test)
	results(common, double, in_to_test, sample)

if __name__ == '__main__':
	main()
