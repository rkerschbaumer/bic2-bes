/**
 * @file myfind.c
 * Betriebssysteme Main file for myfind, which is a reduced find version of Linux.
 * Example 1
 *
 * @author Andrea Maierhofer (aka Windows fangirl) <andrea.maierhofer@technikum-wien.at>
 * @author Reinhard Mayr <reinhard.mayr@technikum-wien.at>
 * @author Thomas Schmid <thomas.schmid@technikum-wien.at>
 * @date 2015/03/13
 *
 * @version SVN $Revision: 99$*
 *
 */

/*
 * -------------------------------------------------------------- review --
 */

/*
 * -------------------------------------------------------------- includes --
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <fnmatch.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

/*
 * --------------------------------------------------------------- defines --
 */
/** DEBUG_OUTPUT 0 is without debug_print(), else debug_print() function is active. */
#define DEBUG_OUTPUT 0

/*
 * -------------------------------------------------------------- typedefs --
 */

/**
 * The struct type for stat return value.
 */
typedef struct stat StatType;

/**
 * The enumeration addition for bool type.
 */
typedef enum booleanEnum
{
    /** Boolean false. */
    FALSE,
    /** Boolean true. */
    TRUE
} boolean;

/*
 * --------------------------------------------------------------- globals --
 */

/*
 * --------------------------------------------------------------- static --
 */

/** Buffer for reading current directory/file. */
static char* spath_buffer = NULL;

/** Buffer for getting the base name of a given directory. */
static char* sbasename_buffer = NULL;

/** Maximum path length of file system. */
static long int smax_path = 0;

/** Current program arguments. */
static const char* sprogram_arg0 = NULL;

/** Maximum buffer size for print buffer.*/
static const long MAX_PRINT_BUFFER = 1000;

/** Print buffer for printout on stderr. */
static char* sprint_buffer = NULL;

/** Want to convert user id number into decimal number. */
static const int USERID_BASE = 10;

/** User text string for supported parameter user. */
static const char* PARAM_STR_USER = "-user";
/** User text string for supported parameter nouser. */
static const char* PARAM_STR_NOUSER = "-nouser";
/** User text string for supported parameter name. */
static const char* PARAM_STR_NAME = "-name";
/** Output string for supported parameter path. */
static const char* PARAM_STR_PATH = "-path";
/** User text for supported parameter type. */
static const char* PARAM_STR_TYPE = "-type";
/** Possible flags set by user for supported parameter type. */
static const char* PARAM_STR_TYPE_VALS = "bcdflps";
/** User text for supported parameter ls. */
static const char* PARAM_STR_LS = "-ls";
/** User text for supported parameter user. */
static const char* PARAM_STR_PRINT = "-print";

/** The user has given a directory after the program name. */
static int parameter_directory_given = TRUE;

/* ------------------------------------------------------------- functions --
 */

#if DEBUG_OUTPUT
void
debug_print(const char* message);
#else /* DEBUG_OUTPUT */
/* suppress debug_print output */
void debug_print(__attribute__((unused))const char* message)
{
}
#endif /* DEBUG_OUTPUT */

inline static int get_max_path_length(void);
inline static char* get_print_buffer(void);
inline static const char* get_program_argument_0(void);
inline static char* get_path_buffer(void);

static void print_usage(void);
static void print_error(const char* message);
static int init(const char** program_args);
static void cleanup(boolean exit);

static int do_file(const char* file_name, StatType* file_info, const char* const* params);
static int do_dir(const char* dir_name, const char* const* params);

static boolean user_exist(const char* user_name, const boolean search_for_uid);
static boolean has_no_user(StatType* file_info);

static char get_file_type(const StatType* file_info);

static boolean filter_name(const char* path_to_examine, const int current_param,
        const char* const* params);
static boolean filter_path(const char* path_to_examine, const int current_param,
        const char* const* params);
static boolean filter_nouser(StatType* file_info);
static boolean filter_user(const int current_param, const char* const* params, StatType* file_info);
static boolean filter_type(const int current_param, const char* const* params, StatType* file_info);

static void print_file_change_time(const StatType* file_info);
static void print_file_permissions(const StatType* file_info);
static void print_user_group(const StatType* file_info);

static void print_detail_ls(const char* file_path, StatType* file_info);
static void print_detail_print(const char* file_path);
static void combine_ls(const StatType* file_info);

/**
 *
 * \brief main implements a a simple replacement for Linux find.
 *
 * This is the main entry point for any C program.
 *
 * \param argc the number of arguments.
 * \param argv the arguments itself (including the program name in argv[0]).
 *
 * \return EXIT_SUCCESS on success  EXIT_FAILURE on error.
 * \retval EXIT_SUCCESS Program ended successfully.
 * \retval EXIT_FAILURE Program ended with failure.
 */
int main(int argc, const char* argv[])
{
    int result = EXIT_FAILURE;
    char* start_dir = NULL;
    char* found_dir = NULL;
    boolean path_given = FALSE;

    StatType stbuf;
    int current_argument = 1; /* the first argument is the program name anyway */
    int test_char = '\0';

    result = init(argv);
    if (EXIT_SUCCESS != result)
    {
        cleanup(TRUE);
    }

    if (argc <= 1)
    {
        print_usage();
        return EXIT_SUCCESS;
    }
    test_char = *argv[1];
    path_given = (test_char == '-') ? FALSE : TRUE;

    /* check the input arguments first */
    while (current_argument < argc)
    {
        if (0 == strcmp(PARAM_STR_USER, argv[current_argument]))
        {
            /* found -user */
            if ((current_argument + 1) < argc)
            {
                current_argument += 2;
                continue;
            }
            else
            {
                print_error("Missing argument to `-user'.");
                cleanup(TRUE);
            }
        }

        if (0 == strcmp(PARAM_STR_NOUSER, argv[current_argument]))
        {
            /* found -nouser */
            current_argument += 1;
            continue;
        }
        if (0 == strcmp(PARAM_STR_LS, argv[current_argument]))
        {
            /* found -ls */
            current_argument += 1;
            continue;
        }
        if (0 == strcmp(PARAM_STR_PRINT, argv[current_argument]))
        {
            /* found -print */
            current_argument += 1;
            continue;
        }
        if (0 == strcmp(PARAM_STR_NAME, argv[current_argument]))
        {
            /* found -name */
            if (argc > (current_argument + 1))
            {
                current_argument += 2;
                continue;
            }
            else
            {
                print_error("Missing argument to `-name'.");
                cleanup(TRUE);
            }
        }

        if (0 == strcmp(PARAM_STR_PATH, argv[current_argument]))
        {
            /* found -path */
            if (argc > (current_argument + 1))
            {
                current_argument += 2;
                continue;
            }
            else
            {
                print_error("Missing argument to `-path'.");
                cleanup(TRUE);
            }
        }

        if (0 == strcmp(PARAM_STR_TYPE, argv[current_argument]))
        {
            /* found -type */
            if (argc > (current_argument + 1))
            {
                const char* next_argument = argv[current_argument + 1];
                if (strlen(next_argument) > 1)
                {
                    snprintf(get_print_buffer(), MAX_PRINT_BUFFER,
                            "Argument of -type must be one character of these `%s'.",
                            PARAM_STR_TYPE_VALS);
                    print_error(get_print_buffer());
                    return EXIT_FAILURE;
                }

                test_char = (int) (*next_argument);
                if (NULL == strchr(PARAM_STR_TYPE_VALS, test_char))
                {
                    snprintf(get_print_buffer(), MAX_PRINT_BUFFER,
                            "Argument -type unknown options of %s: %c.", PARAM_STR_TYPE,
                            *next_argument);
                    print_error(get_print_buffer());
                    return EXIT_FAILURE;
                }
                current_argument += 2;
                continue;
            }
            else
            {
                print_error("Missing argument to `-type'.");
                cleanup(TRUE);
            }
        }

        if (current_argument > 1)
        {
            /* we have an unknown option */
            snprintf(get_print_buffer(), MAX_PRINT_BUFFER, "Invalid predicate `%s'.",
                    argv[current_argument]);
            print_error(get_print_buffer());
            cleanup(TRUE);
        }
        ++current_argument;
    }

    /* determine the directory for start */
    get_path_buffer()[0] = '\0';
    start_dir = (char*) malloc(get_max_path_length() * sizeof(char));
    if (NULL == start_dir)
    {
        free(start_dir);
        start_dir = NULL;
        print_error("malloc() failed: Out of memory.");
        cleanup(TRUE);
    }

    /* build complete path to file (DIR/FILE) */
    snprintf(get_path_buffer(), get_max_path_length(), "%s", argv[1]);

    parameter_directory_given = TRUE;
    /*get information about the file and catch errors*/
    if (!path_given)
    {
        /* no search path defined - we set it to work directory and start */
        parameter_directory_given = FALSE;
        result = do_dir(".", argv);
    }
    else if (-1 != lstat(get_path_buffer(), &stbuf))
    {
        /*search path defined */
        result = do_file(argv[1], &stbuf, argv);
        if (S_ISDIR(stbuf.st_mode))
        {
            found_dir = get_path_buffer();
            strcpy(start_dir, found_dir);
            /* this condition should not happen, but its better to be safe */
            if (NULL == found_dir)
            {
                /* use current directory */
                parameter_directory_given = FALSE;
                strcpy(start_dir, ".");
            }
            result = do_dir(start_dir, argv);
        }
    }

    /* cleanup */
    free(start_dir);
    start_dir = NULL;
    cleanup(FALSE);

    return result;
}

#if DEBUG_OUTPUT != 0
/**
 *
 * \brief debug_output prints debug messages.
 *
 * Does not append \n to message output.
 *
 * \param message output on stdout.
 * \return void
 *
 */
void debug_print(const char* message)
{
    int written = 0;

    written = printf("DGB: %s", message);
    if (written < 0)
    {
        fprintf(stderr, "DBG: debug_print() failed.");
    }

}
#endif /* DEBUG_OUTPUT != 0 */

/**
 *
 * \brief Get maximum path length of this Linux file system.
 *
 * \return Maximum path length.
 */
inline static int get_max_path_length(void)
{
    return smax_path;
}

/**
 *
 * \brief Get program argument0 as string.
 *
 * \return Buffer for printing.
 */
inline static char* get_print_buffer(void)
{
    return sprint_buffer;
}

/**
 *
 * \brief Get program argument0 as string.
 *
 * \return Program name including path.
 */
inline static const char* get_program_argument_0(void)
{
    return sprogram_arg0;
}

/**
 *
 * \brief Get buffer for retrieving the path/directory information.
 *
 * \return Buffer for path information.
 */
inline static char* get_path_buffer(void)
{
    return spath_buffer;
}

/**
 *
 * \brief Get buffer where to write the basename of a path.
 *
 * \return Buffer for path information.
 */
inline static char* get_base_name_buffer(void)
{
    return sbasename_buffer;
}

/**
 *
 * \brief Print the usage.
 *
 * \return void
 */
static void print_usage(void)
{
    int written = 0;

    written = printf("Usage: %s <directory> <test-action> ...\n", get_program_argument_0());
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = printf("Arguments: -user <username|userid>\n");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = printf("           -nouser\n");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = printf("           -type [bcdpfls]\n");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = printf("           -name <glob-pattern>\n");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = printf("           -path <glob-pattern>\n");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = printf("           -print\n");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = printf("           -ls\n");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
}

/**
 *
 * \brief Iterates through directory.
 *
 * \param dir_name directory where to iterate through.
 * \param params is the program argument vector.
 *
 * \return void
 */
static int do_dir(const char* dir_name, const char* const* params)
{
    DIR* dirhandle = NULL;
    struct dirent* dirp = NULL;

    /*open directory catch error*/
    dirhandle = opendir(dir_name);
    if (NULL == dirhandle)
    {
        snprintf(get_print_buffer(), MAX_PRINT_BUFFER, "`%s': %s", dir_name, strerror(errno));
        print_error(get_print_buffer());
        return EXIT_SUCCESS;
    }

    errno = 0;
    while ((dirp = readdir(dirhandle)))
    {
        /* fetch each file from directory, until pointer is NULL */
        StatType file_info;

        if ((strcmp(dirp->d_name, ".") == 0) || (strcmp(dirp->d_name, "..") == 0))
        {
            /* '.' and '..' are not interesting */
            continue;
        }

        /* build complete path to file (DIR/FILE) */
        snprintf(get_path_buffer(), get_max_path_length(), "%s/%s", dir_name, dirp->d_name);
        /* get information about the file and catch errors */
        if (-1 == lstat(get_path_buffer(), &file_info))
        {
            snprintf(get_print_buffer(), MAX_PRINT_BUFFER, "`%s': %s", get_path_buffer(),
                    strerror(errno));
            print_error(get_print_buffer());
            /* check next file */
            continue;
        }

        if (S_ISDIR(file_info.st_mode))
        {
            char* next_path = NULL;
            do_file(get_path_buffer(), &file_info, params);

#if DEBUG_OUTPUT
            snprintf(get_print_buffer(), MAX_PRINT_BUFFER,
                    "Move into directory %s.\n", dirp->d_name);
#endif /* DEBUG_OUTPUT */
            debug_print(get_print_buffer());
            /* recursion for each directory in current directory */
            next_path = (char*) malloc(get_max_path_length() * sizeof(char));
            if (NULL == next_path)
            {
                print_error("malloc() failed: Out of memory.");
                if (closedir(dirhandle) < 0)
                {
                    snprintf(get_print_buffer(), MAX_PRINT_BUFFER, "`%s': closedir() failed: %s.",
                            dir_name, strerror(errno));
                    print_error(get_print_buffer());
                }
                return EXIT_FAILURE;
            }
            strcpy(next_path, get_path_buffer());
            if (EXIT_FAILURE == do_dir(next_path, params))
            {
                if (closedir(dirhandle) < 0)
                {
                    snprintf(get_print_buffer(), MAX_PRINT_BUFFER, "`%s': closedir() failed: %s.",
                            dir_name, strerror(errno));
                    print_error(get_print_buffer());
                }
                free(next_path);
                return EXIT_FAILURE;
            }
            free(next_path);
        }
        else
        {
            do_file(get_path_buffer(), &file_info, params);
        }
        errno = 0; /* reset errno for next call to readdir() */
    }
    if (0 != errno)
    {
        snprintf(get_print_buffer(), MAX_PRINT_BUFFER, "`%s': readdir() failed: %s.", dir_name,
                strerror(errno));
        print_error(get_print_buffer());
    }

    if (closedir(dirhandle) < 0)
    {
        snprintf(get_print_buffer(), MAX_PRINT_BUFFER, "`%s':closedir() failed: %s.", dir_name,
                strerror(errno));
        print_error(get_print_buffer());
    }

    return EXIT_SUCCESS;

}

/**
 *
 * \brief Handle the file.
 *
 * \param file_name is the filename which has to be checked against the find options.
 * \param file_info file information of file_name which has to be checked against the find options.
 * \param params is the program argument vector.
 *
 * \return int represents the exit status of do_file.
 * \retval EXIT_SUCCESS successful exit status.
 * \retval EXIT_FAILURE failing exit status.
 */
static int do_file(const char* file_name, StatType* file_info, const char* const* params)
{

    int i = 1;
    boolean printed_print = FALSE; /* flag for: already printed */
    boolean printed_ls = FALSE; /* flag for: already printed in ls mode*/
    boolean to_print_ls = FALSE; /* flag for: must be printed in ls mode*/
    boolean matched = FALSE; /* flag for: line meets filter criteria */
    boolean filter = FALSE; /* flag for: result of filter */
    boolean filtered = FALSE; /* flag for: at least one filter has been applied */
    boolean path_given = FALSE; /* flag for: first commandline parameter is a file/dir */
    char test_char = '\0';

    /*check if first command line parameter is an option or a file/dir*/
    test_char = *params[1];
    path_given = ('-' == test_char) ? FALSE : TRUE;
    matched = path_given;

    /* loop all command line parameters */
    while (NULL != params[i])
    {
        /* could be later optimzed by remembering for each params[i] element, if it
         * is matched for the i-th parameter, store for each filter in an bool filter_array[argc]
         * if strcmp matched for the filter e.g for type cmp_filter_type[arg]
         */
        /* apply filters */
        if (strcmp(params[i], PARAM_STR_TYPE) == 0)
        {
            filter = filter_type(i, params, file_info);
            matched = matched && filter;
            filtered = TRUE;
            ++i;
            continue;
        }

        if (strcmp(params[i], PARAM_STR_USER) == 0)
        {
            filter = filter_user(i, params, file_info);
            matched = matched && filter;
            filtered = TRUE;
            ++i;
            continue;
        }

        if (strcmp(params[i], PARAM_STR_NOUSER) == 0)
        {
            filter = filter_nouser(file_info);
            matched = matched && filter;
            filtered = TRUE;
            ++i;
            continue;
        }

        if (strcmp(params[i], PARAM_STR_NAME) == 0)
        {
            filter = filter_name(file_name, i, params);
            matched = matched && filter;
            filtered = TRUE;
            ++i;
            continue;
        }

        if (strcmp(params[i], PARAM_STR_PATH) == 0)
        {
            filter = filter_path(file_name, i, params);
            matched = matched && filter;
            filtered = TRUE;
            ++i;
            continue;
        }

        /* apply actions */
        if (strcmp(params[i], PARAM_STR_LS) == 0)
        {
            if (filtered && matched)
            {
                print_detail_ls(file_name, file_info);
                printed_ls = TRUE;
            }
            else
            {
                /* special case -ls defined before filter parameters */
                to_print_ls = TRUE;
            }
        }
        if (strcmp(params[i], PARAM_STR_PRINT) == 0)
        {
            if (filtered && matched)
            {
                print_detail_print(file_name);
                printed_print = TRUE;
            }
        }

        ++i;
    }

    /* special cases */
    /* no -print action or no filter parameter on command line */
    if ((matched && !printed_print && !printed_ls) || (!filtered))
    {
        if (to_print_ls)
        {
            print_detail_ls(file_name, file_info);
        }
        else
        {
            print_detail_print(file_name);
        }
    }

    return EXIT_SUCCESS;
}

/**
 * \brief Initializes the program.
 *
 * \param program_args is the program argument vector.
 *
 * \return EXIT_SUCCESS the program was successfully initialized,
 *  otherwise program startup failed.
 * \retval ENOMEM posix error out of memory.
 * \retval ENODATA posix error ENODATA no data available for maximum path length.
 */
int init(const char** program_args)
{
    sprogram_arg0 = program_args[0];

    if (NULL == sprint_buffer)
    {
        sprint_buffer = (char*) malloc(MAX_PRINT_BUFFER * sizeof(char));
        if (NULL == sprint_buffer)
        {
            fprintf(stderr, "%s: %s\n", sprogram_arg0, "Out of memory.\n");
            return ENOMEM;
        }
    }

    /* get maximum directory size */
    smax_path = pathconf(".", _PC_PATH_MAX);
    if (-1 == smax_path)
    {
        smax_path = 0;
        snprintf(get_print_buffer(), MAX_PRINT_BUFFER, "pathconf() () failed: %s.",
                strerror(errno));
        print_error(get_print_buffer());
        return ENODATA;
    }

    if (NULL == spath_buffer)
    {
        spath_buffer = (char*) malloc(smax_path * sizeof(char));
        if (NULL == spath_buffer)
        {
            print_error("malloc() failed: Out of memory.");
            return ENOMEM;
        }
    }
    if (NULL == sbasename_buffer)
    {
        sbasename_buffer = (char*) malloc(smax_path * sizeof(char));
        if (NULL == sbasename_buffer)
        {
            print_error("malloc() failed: Out of memory.");
            return ENOMEM;
        }
    }

    return EXIT_SUCCESS;

}

/**
 * \brief Cleanup the program.
 *
 * \param exit_program when set to TRUE exit program immediately with EXIT_FAILURE.
 *
 * \return void
 */
void cleanup(boolean exit_program)
{
    free(spath_buffer);
    spath_buffer = NULL;

    free(sbasename_buffer);
    sbasename_buffer = NULL;

    free(sprint_buffer);
    sprint_buffer = NULL;

    fflush(stderr);
    fflush(stdout);

    if (exit_program)
    {
        exit(EXIT_FAILURE);
    }

}

/**
 *
 * \brief Prints error message to stderr.
 *
 * A new line is printed after the message text automatically.
 *
 * \param message output on stderr.
 *
 * \return void
 */
void print_error(const char* message)
{
    int written = 0;

    written = fprintf(stderr, "%s: %s\n", get_program_argument_0(), message);
    if (written < 0)
    {
        /* sorry we can not print to error stream */
        cleanup(TRUE);
    }
}

/**
 *\brief Determines if the given user exists.
 *
 * Check if the given user exist as an user on the system.
 *
 *\param user_name to be queried.
 *\param search_for_uid search for uid if user not found.
 *\
 *\return FALSE user does not exist, TRUE user exists.
 */
static boolean user_exist(const char* user_name, const boolean search_for_uid)
{
    struct passwd* pwd = NULL;
    char* end_userid = NULL;
    uid_t uid = 0;

    pwd = getpwnam(user_name);

    if (NULL != pwd)
    {
        /* the user exist */
        return TRUE;
    }
    else if (FALSE == search_for_uid)
    {
        return FALSE;
    }

    /* is it a user id instead of a user name? */
    errno = 0;
    uid = (uid_t) strtol(user_name, &end_userid, USERID_BASE);
    if (errno)
    {
        return FALSE;
    }
    if (0 == uid)
    {
        return FALSE;
    }

    pwd = getpwuid(uid);
    if (NULL == pwd)
    {
        return FALSE;
    }

    return TRUE;

}

/**
 * \brief Test if file has no user.
 *
 * \param file_info to test if this file has no user.
 *
 * \return FALSE File has a user, TRUE file has no user in user id data base.
 */
static boolean has_no_user(StatType* file_info)
{
    struct passwd* pwd = NULL;

    pwd = getpwuid(file_info->st_uid);
    return (pwd == NULL);
}

/**
 * \brief Query file type of given file.
 *
 * \param file_info as from file system.
 *
 * \return char representing file type.
 */
static char get_file_type(const StatType* file_info)
{

    char result = '-';

    if (S_ISBLK(file_info->st_mode))
    {
        result = 'b';
    }
    else if (S_ISREG(file_info->st_mode))
    {
        result = 'f';
    }
    else if (S_ISCHR(file_info->st_mode))
    {
        result = 'c';
    }
    else if (S_ISDIR(file_info->st_mode))
    {
        result = 'd';
    }
    else if (S_ISFIFO(file_info->st_mode))
    {
        result = 'p';
    }
    else if (S_ISLNK(file_info->st_mode))
    {
        result = 'l';
    }
    else if (S_ISSOCK(file_info->st_mode))
    {
        result = 's';
    }
    return result;

}

/**
 * \brief Filters the directory entry due to -name  parameter.
 *
 * Applies -name filter (if defined) to path_to_examine.
 *
 * \param path_to_examine directory entry to investigate for name.
 * \param current_param currently processed parameter index.
 * \param params Program parameter arguments given by user.
 *
 * \return boolean result indicating filter has matched.
 * \retval TRUE name filter matched or not given.
 * \retval FALSE no match found.
 */
static boolean filter_name(const char* path_to_examine, const int current_param,
        const char* const* params)
{
    char* buffer = NULL;

    /*  We match the actual file path against the pattern
     *  delivered as argument to -name
     */
    buffer = strcpy(get_base_name_buffer(), path_to_examine);
    buffer = basename(buffer);
    /* Do we have a pattern match? */
    return (0 == fnmatch(params[current_param + 1], buffer, 0));
}

/**
 * \brief Filters the directory entry due to -path parameter.
 *
 * Applies -name filter (if defined) to path_to_examine.
 *
 * \param path_to_examine directory entry to investigate for path.
 * \param current_param currently processed parameter index.
 * \param params Program parameter arguments given by user.
 *
 * \return boolean result indicating filter has matched.
 * \retval TRUE name filter matched or not given.
 * \retval FALSE no match found.
 */
static boolean filter_path(const char* path_to_examine, const int current_param,
        const char* const* params)
{
    char* buffer = NULL;

    /**
     *  We match the actual file path against the pattern
     *  delivered as argument to -path
     */
    buffer = strcpy(get_base_name_buffer(), path_to_examine);

    /* Do we have a pattern match? */
    return (0 == fnmatch(params[current_param + 1], buffer, FNM_PATHNAME));
}

/**
 * \brief Filters the directory entry due to -nouser parameter.
 *
 * Applies -nouser filter (if defined) to file_info.
 *
 * \param file_info as read from operating system.
 *
 * \return boolean TRUE name filter matched or not given, FALSE no match found.
 */
static boolean filter_nouser(StatType* file_info)
{
    boolean result = FALSE;

    result = has_no_user(file_info);
    return (result);
}

/**
 * \brief Filters the directory entry due to -user parameter.
 *
 * Applies -user filter (if defined) to file_info.
 *
 * \param current_param currently processed parameter index.
 * \param params Program parameter arguments given by user.
 * \param file_info as read from operating system.
 *
 * \return boolean result indicating filter has matched.
 * \retval TRUE name filter matched or not given.
 * \retval FALSE no match found.
 */
static boolean filter_user(const int current_param, const char* const* params, StatType* file_info)
{
    unsigned int search_uid = 0;
    char * end_ptr = NULL;
    struct passwd* pwd = NULL;

    search_uid = strtol(params[current_param + 1], &end_ptr, USERID_BASE);
    if (('\0' != *end_ptr) || user_exist(params[current_param + 1], FALSE))
    {
        /*  string to int conversion failed --> we have a username */
        /*  or special case --> username is pure numeric */
        if (user_exist(params[current_param + 1], FALSE))
        {
            pwd = getpwuid(file_info->st_uid);
            if (pwd != NULL)
            {
                if (strcmp(pwd->pw_name, params[current_param + 1]) == 0)
                {
                    /* parameter of -user is equal to
                     * user name derived from UID */
                    return TRUE;
                }
            }
            return FALSE;
        }
        else
        {
            snprintf(get_print_buffer(), MAX_PRINT_BUFFER, "`%s' is not the name of a known user",
                    params[current_param + 1]);
            print_error(get_print_buffer());
            cleanup(TRUE);
            return FALSE;
        }
    }
    else
    {
        /* successfull string to int conversion */
        /* -> parameter of -user seems to be an UID */
        return (search_uid == file_info->st_uid);
    }
    return TRUE;
}

/**
 * \brief Filters the directory entry due to -type parameter.
 *
 * Applies -type filter (if defined) to file_info.
 *
 * \param current_param currently processed parameter index.
 * \param params Program parameter arguments given by user.
 * \param file_info as read from operating system.
 *
 * \return boolean result indicating filter has matched.
 * \retval TRUE name filter matched or not given
 * \retval FALSE no match found.
 */

static boolean filter_type(const int current_param, const char* const* params, StatType* file_info)
{
    const char* parameter1 = NULL;

    parameter1 = params[current_param + 1];
    /* check if option argument describes the same file type as file to examine has */
    return (*parameter1 == get_file_type(file_info));
}

/**
 * \brief Print out last changed date of file on standard output.
 *
 * \param file_info with the file attributes.
 *
 * \return void
 **/
void print_file_change_time(const StatType* file_info)
{
    char * buffer_char = NULL;
    int i = 0;
    int written = 0;
    size_t written_time = 0;

    /* Convert the time into the local time format it. */
    written_time = strftime(get_print_buffer(), MAX_PRINT_BUFFER - 1, "%b %d %H:%M",
            localtime(&file_info->st_mtime));
    if (0 == written_time)
    {
        snprintf(get_print_buffer(), MAX_PRINT_BUFFER,
                "strftime() failed: Could not print file changed time\n.");
        print_error(get_print_buffer());
        return;
    }

    /* as strftime Format Parameter %e is not supported on Annubis
     * we have to adjust the remove leading 0 in the day */
    buffer_char = get_print_buffer();
    for (i = 0; i < MAX_PRINT_BUFFER; i++)
    {
        if (' ' == buffer_char[i])
        {
            if ('0' == buffer_char[i + 1])
            {
                buffer_char[i + 1] = ' ';
            }
            break;
        }
    }

    written = fprintf(stdout, "%s", get_print_buffer());
    if (written < 0)
    {
        print_error("fprintf() failed: Could not write time to stdout.\n");
    }

}

/**
 * \brief Print the file permissions on standard output.
 *
 * \param file_info with all file attributes read out from operating system.
 *
 * \return void
 **/
void print_file_permissions(const StatType* file_info)
{
    /* Print file type */
    int written = 0;
    char file_type_character = '\0';

    file_type_character = get_file_type(file_info);
    if (file_type_character == 'f')
    {
        file_type_character = '-';
    }

    written = fprintf(stdout, "%c", file_type_character);
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    /* Print user permissions */
    written = fprintf(stdout, "%c", (file_info->st_mode & S_IRUSR ? 'r' : '-'));
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    written = fprintf(stdout, "%c", (file_info->st_mode & S_IWUSR ? 'w' : '-'));
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    if (!(file_info->st_mode & S_ISUID))
    {
        /*no UID-Bit */
        written = fprintf(stdout, "%c", (file_info->st_mode & S_IXUSR ? 'x' : '-'));
    }
    else if ((file_info->st_mode & S_ISUID) && (file_info->st_mode & S_IXUSR))
    {
        /* UID-Bit && Execute-Bit */
        written = fprintf(stdout, "%c", (file_info->st_mode & S_ISUID ? 's' : '-'));
    }
    else
    {
        /* UID-Bit && !Execute-Bit */
        written = fprintf(stdout, "%c", (file_info->st_mode & S_ISUID ? 'S' : '-'));
    }
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    /* Print group permissions */
    written = fprintf(stdout, "%c", (file_info->st_mode & S_IRGRP ? 'r' : '-'));
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = fprintf(stdout, "%c", (file_info->st_mode & S_IWGRP ? 'w' : '-'));
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    if (!(file_info->st_mode & S_ISGID))
    {
        /* no GID-Bit */
        written = fprintf(stdout, "%c", (file_info->st_mode & S_IXGRP ? 'x' : '-'));
    }
    else if ((file_info->st_mode & S_ISGID) && (file_info->st_mode & S_IXGRP))
    {
        /* GID-Bit && Execute-Bit */
        written = fprintf(stdout, "%c", (file_info->st_mode & S_ISGID ? 's' : '-'));
    }
    else
    {
        /* GID-Bit && !Execute-Bit */
        written = fprintf(stdout, "%c", (file_info->st_mode & S_ISGID ? 'S' : '-'));
    }
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    /* Print other permissions */
    written = fprintf(stdout, "%c", (file_info->st_mode & S_IROTH ? 'r' : '-'));
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = fprintf(stdout, "%c", (file_info->st_mode & S_IWOTH ? 'w' : '-'));
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    if (!(file_info->st_mode & S_ISVTX))
    {
        /* Sticky-Bit */
        written = fprintf(stdout, "%c", (file_info->st_mode & S_IXOTH ? 'x' : '-'));
    }
    else if ((file_info->st_mode & S_ISVTX) && (file_info->st_mode & S_IXOTH))
    {
        /* Sticky-Bit && Execute-Bit */
        written = fprintf(stdout, "%c", (file_info->st_mode & S_ISVTX ? 't' : '-'));
    }
    else
    {
        /* Sticky-Bit && !Execute-Bit */
        written = fprintf(stdout, "%c", (file_info->st_mode & S_ISVTX ? 'T' : '-'));
    }
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    written = fprintf(stdout, "  ");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
}

/**
 * \brief Print user name and group name to standard out.
 *
 * \param file_info with all file attributes read out from operating system.
 *
 * \return void
 **/
static void print_user_group(const StatType* file_info)
{
    struct passwd* password;
    struct group* group_info;
    int written = 0;

    /* Print user name */
    password = getpwuid(file_info->st_uid);
    if (NULL != password)
    {
        written = fprintf(stdout, "%5s", password->pw_name);
    }
    else
    {
        written = fprintf(stdout, "%7d", file_info->st_uid);
    }
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    /* Print group name */
    group_info = getgrgid(file_info->st_gid);
    if (NULL != password)
    {
        written = fprintf(stdout, "%9s", group_info->gr_name);
    }
    else if (NULL != group_info)
    {
        written = fprintf(stdout, "%9s", group_info->gr_name);
    }
    else
    {
        written = fprintf(stdout, "%9d", file_info->st_gid);
    }
    if (written < 0)
    {
        print_error(strerror(errno));
    }

}

/**
 * \brief Print the detailed info of matched file to standard out.
 *
 * \param file_path Fully qualified file name with path read out from operating system.
 * \param file_info with all file attributes read out from operating system.
 *
 * \return void
 **/
static void print_detail_ls(const char* file_path, StatType* file_info)
{
    int written = 0;

    combine_ls(file_info);
    written = printf(" ");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = printf("%s\n", file_path);
    if (written < 0)
    {
        print_error(strerror(errno));
    }
}

/**
 * \brief Standard print, used by every match. Prints on standard out.
 *
 * \param file_path Fully qualified file name with path read out from operating system.
 *
 * \return void
 **/
static void print_detail_print(const char* file_path)
{
    int written = 0;
    written = printf("%s\n", file_path);
    if (written < 0)
    {
        print_error(strerror(errno));
    }
}

/**
 * \brief Print on standard out the -ls arguments: number of i-nodes,blocks, permissions,
 number of links, owner, group, last modification time and directory name.
 symlinks.
 *
 * \param file_info with all file attributes read out from operating system.
 *
 * \return void
 **/
static void combine_ls(const StatType* file_info)
{
    int written = 0;

    /* Print i-node */
    written = fprintf(stdout, "%6lu", (unsigned long) file_info->st_ino);
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    /* Print number of blocks */
    /* magic number divide by 2 depends on block size of file system.
     * The st_blocks member of the stat structure returns:
     * The total number of physical blocks of size 512 bytes actually allocated on disk.
       see also http://stackoverflow.com/questions/1346807/how-does-stat-command-calculate-the-blocks-of-a-file
    */
    written = fprintf(stdout, "%5lu ", (unsigned long) file_info->st_blocks / 2);
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    print_file_permissions(file_info);

    /* Print number of hard links */
    written = fprintf(stdout, "%2lu", (unsigned long) file_info->st_nlink);
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    print_user_group(file_info);

    /* Print file size */
    written = fprintf(stdout, "%13lu ", (unsigned long) file_info->st_size);
    if (written < 0)
    {
        print_error(strerror(errno));
    }

    print_file_change_time(file_info);
}

/*
 * =================================================================== eof ==
 */

