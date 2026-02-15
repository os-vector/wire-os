#
# Uncompress a file generated via atrace -z
#
# Usage: python atrace-uncompress.py infile > outfile
#
import sys, zlib

def main():

	if len(sys.argv) != 2:
		print >> sys.stderr, ('Usage: %s inputfile' % sys.argv[0])
		sys.exit(1)

	infile = open(sys.argv[1], "rb")
	out = infile.read()
	parts = out.split('\nTRACE:', 1)

	data = ''.join(parts[1])

	# Remove CR characters
	if data.startswith('\r\n'):
		data = data.replace('\r\n', '\n')

	# Skip the initial newline.
	data = data[1:]

	if not data:
		print >> sys.stderr, ('No trace data found')
		sys.exit(1)

	out = zlib.decompress(data)
	print(out)

if __name__ == '__main__':
	main()
