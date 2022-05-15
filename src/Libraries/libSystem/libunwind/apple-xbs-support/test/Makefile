## Makefile to run the `apple-xbs-support` tests.

VENV_OUT=.python_env
PYTHON_ROOT := $(VENV_OUT)/bin/

all: check-lit

clean:
	rm -rf $(VENV_OUT)

install-lit: $(VENV_OUT) Makefile

$(VENV_OUT):
	@echo "Setting up python venv & installing lit..."
	python3 -m venv $(VENV_OUT)
	$(PYTHON_ROOT)pip install lit
	@echo ""

check-lit: install-lit
	@echo "Running 'apple-xbs-support' tests..."
	$(PYTHON_ROOT)lit -vv .

.PHONY: all clean install-lit check-lit
