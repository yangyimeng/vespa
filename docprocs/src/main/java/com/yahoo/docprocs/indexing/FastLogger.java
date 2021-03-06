// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.docprocs.indexing;

import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * @author <a href="mailto:simon@yahoo-inc.com">Simon Thoresen Hult</a>
 */
class FastLogger {

    private final Logger log;

    private FastLogger(Logger log) {
        this.log = log;
    }

    public void log(Level level, String format, Object... args) {
        if (!log.isLoggable(level)) {
            return;
        }
        if (args.length > 0) {
            log.log(level, String.format(format, args));
        } else {
            log.log(level, format);
        }
    }

    public static FastLogger getLogger(String name) {
        return new FastLogger(Logger.getLogger(name));
    }
}
