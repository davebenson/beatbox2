
all:
	perl build-scripts/compile.pl
	@echo "NOTE: run 'make songs' to generate songs (requires 'lame')"

clean:
	rm -rf .generated bb-run test-0

songs:
	@perl build-scripts/compile.pl
	build-scripts/make-all-songs $(OUTPUT_DIR)

dist:
	build-scripts/make-tarball --date

release:
	build-scripts/make-release

bb-run%: this_file_should_never_be_created
	perl build-scripts/compile.pl $@

.PHONY: this_file_should_never_be_created
	
