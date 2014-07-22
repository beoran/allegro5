#ifndef __al_included_allegro5_aintern_sal_h
#define __al_included_allegro5_aintern_sal_h


/* When using the official DirectX or Xinput headers, 
the SAL annotation junk gets in the way, and the msys2 provided sal.h isn't 
eniugh to get rid of the annotations. Therefore we disable
them by defining them here. */   

#if defined __MINGW32__ 
	#define __deref_out
	#define __deref_in
	#define __out_opt
	#define __in_opt
	#define __null
	#define __in
	#define __out
	#define __in_bcount(ignored)
	#define __out_bcount(ignored)
	#define __in_ecount(ignored)
	#define __out_ecount(ignored)
	#define __in_bcount_opt(ignored)
	#define __out_bcount_opt(ignored)
	#define __in_ecount_opt(ignored)
	#define __out_ecount_opt(ignored)
	#define __deref_out_bcount(ignored)
	#define __deref_in_bcount(ignored)
	#define __deref_opt_out_bcount(ignored)
	#define __deref_opt_in_bcount(ignored)
#endif


#endif

/* vim: set ts=8 sts=3 sw=3 et: */
