int
check_complex_float_xmm_spill(_Complex float a, _Complex float b,
	                         _Complex float c, _Complex float d,
	                         _Complex float e, _Complex float f,
	                         _Complex float g, _Complex float h,
	                         _Complex float i)
{
	if ((float)a != 1.5f || (float)b != 2.5f || (float)c != 3.5f ||
	    (float)d != 4.5f || (float)e != 5.5f || (float)f != 6.5f ||
	    (float)g != 7.5f || (float)h != 8.5f || (float)i != 9.5f)
		return 1;
	return 42;
}
