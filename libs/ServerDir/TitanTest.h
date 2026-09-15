//////////////////////////////////////////////////////////////////////////////
// Testing code for playing with the Titan API

#ifndef __TITANTEST_H__
#define __TITANTEST_H__

#ifndef ENABLE_TITAN_TEST

inline void TitanTest_Init() {}
inline void TitanTest_Term() {}

#else // ENABLE_TITAN_TEST

void TitanTest_Init();
void TitanTest_Term();

#endif // ENABLE_TITAN_TEST

#endif //__TITANTEST_H__