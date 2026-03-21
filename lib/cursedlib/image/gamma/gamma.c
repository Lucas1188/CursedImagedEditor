/*
    Computer graphics renderers often do not perform gamma encoding,
    instead making sample values directly proportional to scene light
    intensity.  If the PNG encoder receives sample values that have
    already been quantized into linear-light integer values, there is
    no point in doing gamma encoding on them; that would just result
    in further loss of information.  The encoder should just write the
    sample values to the PNG file.  This "linear" sample encoding is
    equivalent to gamma encoding with a gamma of 1.0, so graphics
    programs that produce linear samples should always emit a gAMA
    chunk specifying a gamma of 1.0.
*/