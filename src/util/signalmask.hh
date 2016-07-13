/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef SIGNALMASK_HH
#define SIGNALMASK_HH

#include <sys/signalfd.h>
#include <initializer_list>

/* wrapper class for Unix signal masks */

class SignalMask
{
private:
    sigset_t mask_;

public:
    SignalMask( const std::initializer_list< int > signals );
    const sigset_t & mask( void ) const { return mask_; }
    void set_as_mask( void ) const;

    static SignalMask current_mask( void );
    bool operator==( const SignalMask & other ) const;
};

#endif /* SIGNALMASK_HH */
