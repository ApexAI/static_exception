// Copyright 2018 Apex.AI, Inc.

#include "SomeClass.hpp"

class SomeException {
  char dummy_data[512];
};

SomeClass::SomeClass() {
  try {
    throw SomeException();
  }
  catch (...) {

  }
};