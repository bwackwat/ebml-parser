#!/bin/python

import re

n = open("index2.html", "w")

with open("index1.html") as f:
	lines = f.readlines()
	name = ""
	id = ""
	type = ""
	for line in lines:
		if "tr id=" in line:
			line = line.split("=")[1]
			line = " ".join(re.findall('"([^"]*)"', line))
			name = line
		if "<td><abbr title=" in line:
			line = line.split("=")[1]
			line = " ".join(re.findall('"([^"]*)"', line))
			if line == "Master Elements":
				line = "MASTER"
			elif line == "Unsigned Integer":
				line = "UINT"
			elif line == "Binary":
				line = "BINARY"
			elif line == "UTF-8":
				line = "UTF8"
			elif line == "String":
				line = "STRING"
			elif line == "Signed Integer":
				line = "INT"
			elif line == "Date":
				line = "DATE"
			elif line == "Float":
				line = "FLOAT"
			n.write("\tnew ebml_element(\"" + name + "\", " + id + ", " + line + "),\n")
		if "<td>[" in line:
			hexs = re.findall(r"\[(\w+)\]", line)
			hexs = [("0x" + hex.lower()) for hex in hexs]
			id = "{{" + ", ".join(hexs) + "}}"
	
	
n.close()	
