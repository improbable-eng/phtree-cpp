valgrind --leak-check=full --error-exitcode=1 --suppressions=tools/runners/sanitizers/valgrind-memcheck/valgrind-suppressions.txt "${@}"
