provider test_provider {
	probe go(int, int, int);
	probe called_after_fork();
};
