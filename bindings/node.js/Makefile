all: install build format test

# cleans typescript dependencies
clean-install:
	rm -rf node_modules

# installs typescript dependencies
install: clean-install
	yarn install --ignore-scripts

# Cleans native dependency, bindings and typescript artifacts
clean:
	rm -rf build
	rm -rf dist
	rm -f *.node
	rm -f *.a
	rm -f *.o
	rm -rf ref-tests

# Cleans and rebuilds native dependencies, bindings and typescript wrapper
build: clean src/kzg.cxx lib/kzg.ts package.json tsconfig.build.json binding.gyp Makefile
	# Prepare the dependencies directory
	mkdir -p deps/c-kzg
	cp -r ../../blst deps
	cp ../../src/c_kzg_4844.c deps/c-kzg
	cp ../../src/c_kzg_4844.h deps/c-kzg
	# Patch the blst_aux.h to fix a compiler error
	awk '{gsub(/typedef struct \{\} blst_uniq;/, "typedef struct { int dummy; } blst_uniq;")}1' \
		deps/blst/bindings/blst_aux.h > blst_aux_temp.h
	mv blst_aux_temp.h deps/blst/bindings/blst_aux.h
	# Build the bindings
	yarn node-gyp configure
	yarn node-gyp build
	yarn tsc -p tsconfig.build.json

# Bundle the distribution, good for distribution validation
bundle:
	mv deps dist
	cp -r src dist/src
	cp README.md dist/README.md
	cp package.json dist/package.json
	cp binding.gyp dist/binding.gyp

# run unit tests and ref-tests
test:
	yarn jest

# lint js/ts code
format:
	yarn prettier --write .

# publish package to npm. required auth token
publish: build
	cd dist
	npm publish

# run ref-tests in linux environment for cross-compatability check
linux-test: build
	# Docker cannot copy from outside this dir
	cp -r ../../tests ref-tests
	docker build -t "linux-test" .
	docker logs --follow `docker run -d linux-test`
	rm -rf ref-tests

leak-check:
	cd ../..; \
	docker build -f bindings/node.js/Dockerfile.valgrind -t "valgrind-node-bindings" .; \
	docker run valgrind-node-bindings
