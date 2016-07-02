.PHONY: all

all: ./js ./css
	@echo "Building Web interface..."
	@rm -rf ./build; \
	mkdir ./build && \
	cd ./js && \
	cat jquery-2.2.4.min.js bootstrap.min.js main.js > combined.js && \
	cd ../ && \
	cd ./css && \
	cat bootstrap.min.css bootstrap-theme.min.css customize.css > combined.css && \
	cd ../ && \
	cp ./index.html ./build/index.html && \
	sed -e "/\/\/\ embed:.\/js\/combined.js/{r ./js/combined.js" -e "d}" ./index.html  > ./build/index.html.temp && \
	sed -e "/\/\*\ embed:.\/css\/combined.css\ \*\//{r ./css/combined.css" -e "d}" ./build/index.html.temp  > ./build/index.html && \
	rm ./js/combined.js; \
	rm ./css/combined.css; \
	rm ./build/index.html.temp
