#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

#include "runner.h"
#include "task.h"

/* Actual tests and helpers are defined in test-list.h */
#include "test-list.h"

ngx_log_t	*global_log;


static ngx_uint_t   test_show_help;
static ngx_uint_t   test_show_list;
static char			*test_prefix;
static char			*test_conf_file;
static char			*test_task_name;
static char			*test_process_name;


static void test_show_help_info(const char *program_name);
static ngx_int_t run_tests_get_options(int argc, char *const *argv);


int main(int argc, char **argv) {
  if (platform_init(argc, argv))
    return EXIT_FAILURE;


  if (run_tests_get_options(argc, argv))
	  return EXIT_FAILURE;


  ngx_time_init();

  ngx_pid = ngx_getpid();

  global_log = ngx_log_init((u_char*)test_prefix);
  if (global_log == NULL) {
      return EXIT_FAILURE;
  }

  if (test_show_help) {
	  test_show_help_info(argv[0]);
	  return EXIT_SUCCESS;
  }

  if (test_show_list) {
	  print_tests(stdout);
	  return EXIT_SUCCESS;
  }

  if (!test_task_name && !test_process_name) {
	  return run_tests(0);
  }

  if (test_task_name && !test_process_name) {
	  return run_test(test_task_name, 0, 1);
  }

  if (test_task_name && test_process_name) {
	  return run_test_part(test_task_name, test_process_name);
  }

  return EXIT_SUCCESS;
}


static ngx_int_t
run_tests_get_options(int argc, char *const *argv)
{
    char     *p;
    ngx_int_t   i;

    for (i = 1; i < argc; i++) {

        p = (char *) argv[i];

        if (*p++ != '-') {
        	log_error("invalid option: \"%s\"", argv[i]);
            return NGX_ERROR;
        }

        while (*p) {

            switch (*p++) {

            case '?':
            case 'h':
                test_show_help = 1;
                break;

            case 'l':
            	test_show_list = 1;
                break;

            case 'p':
                if (*p) {
                	test_prefix = p;
                    goto next;
                }

                if (argv[++i]) {
                	test_prefix = (char *) argv[i];
                    goto next;
                }

                log_error("option \"-p\" requires directory name");
                return NGX_ERROR;

            case 'c':
                if (*p) {
                    test_conf_file = p;
                    goto next;
                }

                if (argv[++i]) {
                	test_conf_file = (char *) argv[i];
                    goto next;
                }

                log_error("option \"-c\" requires file name");
                return NGX_ERROR;

            case 't':
                if (*p) {
                	test_task_name = p;
                    goto next;
                }

                if (argv[++i]) {
                	test_task_name = (char *) argv[i];
                    goto next;
                }

                log_error("option \"-t\" requires task name");
                return NGX_ERROR;

            case 'g':
                if (*p) {
                	test_process_name = p;
                    goto next;
                }

                if (argv[++i]) {
                	test_process_name = (char *) argv[i];
                    goto next;
                }

                log_error("option \"-g\" requires process name");
                return NGX_ERROR;

            default:
            	log_error("invalid option: \"%c\"", *(p - 1));
                return NGX_ERROR;
            }
        }

    next:

        continue;
    }

    return NGX_OK;
}



static void
test_show_help_info(const char *program_name)
{
	log_info("Usage: %s [-?hl] [-c filename] [-p prefix] [-t task_name] [-g process_name]",
			program_name);
}

