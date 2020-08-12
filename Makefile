dev:
	./scripts/build.sh

prod:
	./scripts/build.sh prod

infer:
	./scripts/build.sh infer

test:
	./scripts/test.sh

deploy:
	./scripts/deploy.sh

website:
	./scripts/website.sh

clean:
	rm -rf build thirdparty/asdl/__pycache__/ tests/*.csv

