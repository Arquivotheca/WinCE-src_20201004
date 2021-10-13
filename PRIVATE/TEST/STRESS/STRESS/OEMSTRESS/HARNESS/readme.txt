Usage:  oemstress [options] [commands]

    Oemstress runs in two modes, normal and command. If there's already an
    instance running, subsequent instances of oemstress run in command mode (or
    helper mode). Using command mode, the main instance of the harness can be
    requested to terminate or the logging verbosity level of the main instance
    can be changed dynamically.

    The information regarding stress modules are read from initialization file.
    By default the harness expects the file to be on the host machine, release
    directory (i.e. 'cesh:oemstress.ini').

    No white space is allowed between command line switch and corresponding
    value. File paths with spaces, should be placed inside double quotes.

-result[:[cesh:]result_filepath]
    Enables/Specifies the result file. Using only '-result' has the effect of
    enabling default result file at the default location, i.e cesh:result.xml.
    Prefixing with 'cesh:' creates the file on the host machine, in the release
    directory. If the filename ends with '.xml', the harness automatically
    creates result file in XML format, otherwise it's always text. Result file
    generation is not enabled by default. It's advisable to create the result
    file on non-volatile storage to avoid data loss in case of a device crash.

-history[:[cesh:]history_filepath]
    Enables/Specifies the history file. Using only '-history' has the effect of
    enabling default history file at the default location, i.e cesh:history.csv.
    Prefixing with 'cesh:' creates the file on the host machine, in the release
    directory. Result file generation is not enabled by default. History file
    can becore quite large and hence, creation of the history file on the device
    is not recommended.

-ini:initialization_filepath
    Initialization file is enabled by default and the harness expects the
    default file to be at the default location, i.e. 'cesh:oemstress.ini',
    unless otherwise specified(i.e. 'cesh:oemstress.ini'). Prefixing with
    'cesh:' makes the harness expect the file on the host machine, in the
    release directory.

-ht:hang_trigger_time
    Specifies the time in minutes, after which the harness considers some or
    all of the modules hung based on the hang options. A '0' value disables hang
    detection. If the debugger is attached to the device, hang detection causes
    the harness to 'break' at that specific instance. Otherwise, it simply logs
    all the hung modules (output window and result file only). By default, hang
    trigger time is set to 30 minutes.

-ho:(any|all)
    Option '-ho:any' causes per-module hang detection, i.e. if any module keeps
    running for more than 'hang trigger time', the harness considers it as hang.
    On the other hand, if '-ho:all' option is specified, harness triggers a hang
    only if all the modules keeps running for more than 'hang trigger time'. By
    default, the option is set to '-ho:all'.

-rt:requested_run_time
    Specifies total time in minutes, after which the harness should terminate. A
    '0' value runs stress for indefinite amount of time. By default, it's set to
    900 minutes.

-ri:result_refresh_interval
    Specifies the time interval in minutes, at which the result file (and the
    output) should get refreshed with the latest status. In case of the history
    file the latest status gets appended. By default, 'result refresh interval'
    is set to 5 minutes.

-md:module_duration
    Specifies the amount of time in minutes, each module would be requested to
    run when launched by the harness. Special flag 'random' launches the module
    for random amount of time (maximum module runtime is set to the hang
    trigger time if non-zero, infinite if hang detection is disabled),
    'indefinite' launches the module indefinitely (upto the requested harness
    runtime) and 'minimum' launches each module for only its minimum runtime.
    Using 'indefinite' also has the effect of disabling the hang detection. By
    default, 'module duration' is set to 10 minutes.

-mc:module_count
    Specifies total number of module the harness can run at a given instance. By
    default, 'module count' is set to 4.

-seed:random_seed
    Specifies the random seed to use. By default, the harness uses value
    returned by current tick count.

-vl[:(abort|fail|warning1|warning2|comment|verbose)]
    Available in both normal and command modes. Specifies the verbosity level of
    the module logs. If no option is specified, i.e. '-vl', the harness enables
    full verbosity. By default, the harness enables abort, failure, warning1,
    warning2 and comment logs.

-server:server_name
    Specifies the server name to be passed on to the test modules. By default,
    'server name' is set to 'localhost'.

-terminate
    Available in command mode only. Requests already running instance of the
    harness to terminate.

-wait
	After running for the total requested time, makes the harness wait for the
	completion of the currently running modules before exiting. Has no effect
	if used with -md:INDEFINITE	or -md:RANDOM, defaulted to false.

-autooom
	Enables AutoOOM which might close an application in random and/or reduce
	amount of memory allocated for object store to increase available amount of
	program memory.

-trackmem:(high_watermark%|low_watermark[B|KB|MB])
	Useful only when a debugger is attached to the device. Without a debugger,
	oemstress only logs the fact that the current memory status is beyond the
	specified watermark. If 'high_watermark%' is specified (trailing '%' is
	important), oemstress breaks in debugger if the total memory usage goes
	beyond 'high_watermark' percent value. On the other hand, if 'low_watermark'
	is specified (trailing 'B', 'K', 'KB', 'M', 'MB') oemstress breaks in
	debugger if the available physical memory goes below 'low_watermark' value.
	If no trailing unit is specified, oemstress defaults the specified value to
	low_watermark bytes.
