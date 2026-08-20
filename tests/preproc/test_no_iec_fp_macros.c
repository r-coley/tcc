#ifdef __STDC_IEC_559__
#error __STDC_IEC_559__ should be undefined without IEC 559 floating-point support
#endif

#ifdef __STDC_IEC_559_COMPLEX__
#error __STDC_IEC_559_COMPLEX__ should be undefined without IEC 559 complex support
#endif

#ifdef __STDC_IEC_60559_BFP__
#error __STDC_IEC_60559_BFP__ should be undefined without IEC 60559 binary floating-point support
#endif

#ifdef __STDC_IEC_60559_COMPLEX__
#error __STDC_IEC_60559_COMPLEX__ should be undefined without IEC 60559 complex support
#endif

#ifdef __STDC_IEC_60559_DFP__
#error __STDC_IEC_60559_DFP__ should be undefined without IEC 60559 decimal floating-point support
#endif

int
main(void)
{
	return 0;
}
