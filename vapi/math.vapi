namespace math
{
    [CCode (cname = "M_PI", cheader_filename = "math.h") ]
    public const double M_PI;

    [CCode (cname = "sqrt", cheader_filename = "math.h")]
    public static double sqrt (double val);
}
