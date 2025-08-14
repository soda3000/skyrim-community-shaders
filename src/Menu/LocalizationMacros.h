#pragma once
#include "Localization.h"

// Shorthand for plain lookup
#define TR(ID) ::Loc::Get(ID)

// Shorthand for formatted lookups
#define TRF(ID, MAP) ::Loc::Fmt(ID, MAP)