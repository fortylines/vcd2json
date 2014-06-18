vcd2json
========

A utility to extract variables from a Value Change Dump (VCD) for a specific
time frame. This utility prints a JSON formatted output.

Example
-------

    $ ./vcd2json --name board/clock --start 0 --end 1000 fixtures/board.vcd
    {
    "date": "Sun Nov 11 18:43:39 2012",
    "version": "Icarus Verilog",
    "timescale": "1s",
    "definitions": {
        "board": {
            "clock": "!",
            "count[3:0]": "\"",
            "eSeg": "#",
            "counter": {
                "clock": "!",
                "ctr[3:0]": "$"
            },
            "clockGen": {
                "clock": "%"
            },
            "disp": {
                "A": "&",
                "B": "'",
                "C": "(",
                "D": ")",
                "eSeg": "#",
                "p1": "*",
                "p2": "+",
                "p3": ",",
                "p4": "-"
            }
        }
    },
    "board/clock": [
        [0, "x"],
        [5, "1"],
        [50, "0"],
        [100, "1"],
        [150, "0"],
        [200, "1"],
        [250, "0"],
        [300, "1"],
        [350, "0"],
        [400, "1"],
        [450, "0"],
        [500, "1"],
        [550, "0"],
        [600, "1"],
        [650, "0"],
        [700, "1"],
        [750, "0"],
        [800, "1"],
        [850, "0"],
        [900, "1"],
        [950, "0"]
    ]}

Python Wrapper
--------------

    $ python
    >>> import vcd, json
    >>> with open('fixtures/board.vcd') as f:
    ...     json.loads(vcd.values(f, ['board/clock'], 0, 1000, 1))
    ...

Note you might have to adjust your LD_LIBRARY_PATH or DYLD_LIBRARY_PATH
shell variable to find the dynamic library.


Building
--------

    $ git clone http://github.com/fortylines/vcd2json.git
    $ cd vcd2json
    $ make
    $ make install


