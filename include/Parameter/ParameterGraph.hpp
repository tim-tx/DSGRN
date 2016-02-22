/// ParameterGraph.hpp
/// Shaun Harker
/// 2015-05-24

#ifndef DSGRN_PARAMETERGRAPH_HPP
#define DSGRN_PARAMETERGRAPH_HPP

#ifndef INLINE_IF_HEADER_ONLY
#define INLINE_IF_HEADER_ONLY
#endif

#include "ParameterGraph.h"

INLINE_IF_HEADER_ONLY ParameterGraph::
ParameterGraph ( void ) {
  data_ . reset ( new ParameterGraph_ );
}

INLINE_IF_HEADER_ONLY ParameterGraph::
ParameterGraph ( Network const& network, std::string const& path ) {
  assign ( network, path );
}

INLINE_IF_HEADER_ONLY void ParameterGraph::
assign ( Network const& network, std::string const& path ) {
  data_ . reset ( new ParameterGraph_ );
  data_ -> network_ = network;
  data_ -> reorderings_ = 1;
  data_ -> fixedordersize_ = 1;
  // Load the network files one by one.
  uint64_t D = data_ -> network_ . size ();
  for ( uint64_t d = 0; d < D; ++ d ) {
    uint64_t n = data_ -> network_ . inputs ( d ) . size ();
    uint64_t m = data_ -> network_ . outputs ( d ) . size ();
    data_ -> order_place_values_ . push_back ( _factorial ( m ) );
    data_ -> reorderings_ *= data_ -> order_place_values_ . back ();
    std::vector<std::vector<uint64_t>> const& logic_struct = data_ -> network_ . logic ( d );
    std::stringstream ss;
    ss << path << "/" << n <<  "_" << m;
    for ( auto const& p : logic_struct ) ss <<  "_" << p.size();
    if ( data_ -> network_ . essential ( d ) ) ss << "_E";
    ss << ".dat";
    //std::cout << "Acquiring logic data in " << ss.str() << "\n";
    std::vector<std::string> hex_codes;
    std::ifstream infile ( ss.str() );
    if ( not infile . good () ) {
      throw std::runtime_error ( "Error: Could not find logic resource " + ss.str() + ".\n");
    }
    std::string line;
    while ( std::getline ( infile, line ) ) hex_codes . push_back ( line );
    infile . close ();
    data_ -> factors_ . push_back ( hex_codes );
    data_ -> logic_place_values_ . push_back ( hex_codes . size () );
    data_ -> fixedordersize_ *= hex_codes . size ();
    //std::cout << d << ": " << hex_codes . size () << " factorial(" << m << ")=" << _factorial ( m ) << "\n";
  }
  data_ -> size_ = data_ -> fixedordersize_ * data_ -> reorderings_;
}

INLINE_IF_HEADER_ONLY uint64_t ParameterGraph::
size ( void ) const {
  return data_ -> size_;
}

INLINE_IF_HEADER_ONLY Parameter ParameterGraph::
parameter ( uint64_t index ) const {
  //std::cout << data_ -> "ParameterGraph::parameter( " << index << " )\n";
  if ( index >= size () ) {
    throw std::runtime_error ( "ParameterGraph::parameter Index out of bounds");
  }
  uint64_t logic_index = index % data_ -> fixedordersize_;
  uint64_t order_index = index / data_ -> fixedordersize_;
  //std::cout << "Logic index = " << logic_index << "\n";
  //std::cout << "Order index = " << order_index << "\n";

  uint64_t D = data_ -> network_ . size ();
  std::vector<uint64_t> logic_indices;
  for ( uint64_t d = 0; d < D; ++ d ) {
    uint64_t i = logic_index % data_ -> logic_place_values_ [ d ];
    logic_index /= data_ -> logic_place_values_ [ d ];
    logic_indices . push_back ( i );
  }
  std::vector<uint64_t> order_indices;
  for ( uint64_t d = 0; d < D; ++ d ) {
    uint64_t i = order_index % data_ -> order_place_values_ [ d ];
    order_index /= data_ -> order_place_values_ [ d ];
    order_indices . push_back ( i );
  }
  std::vector<LogicParameter> logic;
  std::vector<OrderParameter> order;
  for ( uint64_t d = 0; d < D; ++ d ) {
    uint64_t n = data_ -> network_ . inputs ( d ) . size ();
    uint64_t m = data_ -> network_ . outputs ( d ) . size ();
    std::string hex_code = data_ -> factors_ [ d ] [ logic_indices[d] ];
    LogicParameter logic_param ( n, m, hex_code );
    OrderParameter order_param ( m, order_indices[d] );
    logic . push_back ( logic_param );
    order . push_back ( order_param );
  }
  Parameter result ( logic, order, data_ -> network_ );
  return result;
}

INLINE_IF_HEADER_ONLY uint64_t ParameterGraph::
index ( Parameter const& p ) const {

  std::vector<LogicParameter> logic = p . logic ( );
  std::vector<OrderParameter> order = p . order ( );

  /// Construct Logic indices
  std::vector<uint64_t> logic_indices;
  uint64_t D = data_ -> network_ . size ();
  for ( uint64_t d = 0; d< D; ++d ) {
      std::string hexcode = logic [ d ] . hex ( );
      /// If factors_ was a different container, we could use find instead
      /// find the hex code within factors_ for the appropriate node
      uint64_t hexsize = data_ -> factors_ [ d ] . size ( );
      for ( uint64_t i = 0; i < hexsize; ++i ) {
          if ( hexcode == data_ -> factors_ [ d ] [ i ] ) {
              logic_indices . push_back ( i );
          }
      }
  }

  /// Construct Order indices
  std::vector<uint64_t> order_indices;
  for ( uint64_t d = 0; d < D; ++ d ) {
      order_indices . push_back ( order[d].index() );
  }

  /// Find logic_index and order_index, in general
  /// notation :
  /// Logic_place_values : lpv
  /// logics_indices : lis
  ///
  /// For the logic index L (but similar for order index)
  /// for various level of d's
  /// d = 0 : lis[0] = L % lpv[0]
  /// d = 1 : lis[1] = floor( L/lpv[0] ) % lpv[1]
  /// d = 2 : lis[2] = floor( floor(L/lpv[0]) / lpv[1] ) % lpv[2]
  /// ...
  /// so for we have :
  ///
  /// from d = 0 :
  /// L = m_0 * lpv[0] + lis[0] and 0 <= L < size()                          (0)
  /// so 0 <= m_0 < floor(size()/lpv[0])
  ///
  /// from d = 1 :
  /// floor(L/lpv[0]) = m_1 * lpv[1] + lis[1]
  /// or
  /// m_0 = m_1 * lpv[1] + lis[1]                                            (1)
  /// a given value m_0 is admissible if there is a value m_1 such that
  /// the equation (1) is satisfied, meaning (m_0-lis[1]) % lpv[1] == 0
  /// if (1) is satisfied, compute m_1 and go to the next level for d.
  /// if (1) is not satisfied, go back to (0) with a new value for m_0
  ///
  /// from d = 2 :
  /// floor( floor(L/lpv[0])/lpv[1] ) =  m_2 * lpv[2] + lis[2]
  /// or
  /// m_1 = m_2 * lpv[2] + lis[2]                                            (3)
  /// the m_1 value found is admissible is (3) can be satisfied,
  /// meaning :  (m_1-lpv[2]) % lpv[2] == 0
  ///
  /// keep going till the last level for d.
  ///
  /// if d = 2 is the last level, and (3) is satisfied then m_0 is an admissible
  /// value and L can be computed using (0). If (3) is not satisfied, then
  /// go back to (0) with a new value for m_0
  ///
  /// In general, a set of values of m_0 will be admissible.
  /// it goes like  :
  /// for 0<index<fixedordersize_                 : "reorderings_" admissible values
  /// for fixedordersize_<index<2*fixedordersize_ : "reorderings_-1" admissible values
  /// ...
  /// for size()-fixedordersize_<index<size()     : we have 1 admissible value
  ///
  /// In practice, the first admissible value for m_0, meaning satisfying all
  /// the different equations for each d-level is the right now. (or so it seems)
  ///

///--- Logic Index ---
  uint64_t logic_index;
  uint64_t nmax = floor(size()/data_ -> logic_place_values_[0]);
  for ( uint64_t i = 0; i < nmax; ++i ) {
    uint64_t d = 1;
    bool flag = true;
    uint64_t R = i;
    while ( (d < D) && flag ) {
      /// careful with negative values and %
      int A = R - logic_indices[d];
      while ( A < 0 ) { A+= data_->logic_place_values_[d]; }
      ///
      if ( A%data_->logic_place_values_[d] == 0 ) {
        R = ( R - logic_indices[d] ) / data_->logic_place_values_[d];
      } else {
        flag = false;
      }
      ++d;
    }
    if ( flag ) {
      logic_index = i * data_ -> logic_place_values_ [ 0 ] + logic_indices [ 0 ];
      break; // Find only the first good value
    }
  }

///--- Order Index ---
  uint64_t order_index;
  nmax = data_->reorderings_;
  for ( uint64_t i = 0; i < nmax; ++i ) {
    uint64_t d = 1;
    bool flag = true;
    uint64_t R = i;

    while ( (d < D) && flag ) {
      /// careful with negative values and %
      int A = R - order_indices[d];
      while ( A < 0 ) { A += data_ -> order_place_values_[d]; }
      ///
      if ( A % data_ -> order_place_values_[d] == 0 ) {
        R = ( R - order_indices [ d ] ) / data_ -> order_place_values_ [ d ];
      } else {
        flag = false;
      }
      ++d;
    }
    if ( flag ) {
      order_index = i *data_ -> order_place_values_ [ 0 ] + order_indices [ 0 ];
      break; // Find only the first good value
    }
  }

  return order_index * data_->fixedordersize_ + logic_index;

}

INLINE_IF_HEADER_ONLY std::vector<uint64_t> ParameterGraph::
adjacencies ( const uint64_t myindex ) const {
  std::vector<uint64_t> output;
  /// Get the parameter from the index
  Parameter p = parameter ( myindex );
  /// Get the logic
  std::vector<LogicParameter> logics = p . logic ( );
  /// get the order
  std::vector<OrderParameter> orders = p . order ( );

  uint64_t D = data_ -> network_ . size ( );

  typedef uint64_t bitType;
  typedef std::vector<bitType> BitContainer;
  // Convert an hex char into a vector of bits (length 4)
  auto Hex2Bin = [] ( const char & c ) -> BitContainer {
    switch ( c ) {
      case '0' : return BitContainer {0,0,0,0};
      case '1' : return BitContainer {0,0,0,1};
      case '2' : return BitContainer {0,0,1,0};
      case '3' : return BitContainer {0,0,1,1};
      case '4' : return BitContainer {0,1,0,0};
      case '5' : return BitContainer {0,1,0,1};
      case '6' : return BitContainer {0,1,1,0};
      case '7' : return BitContainer {0,1,1,1};
      case '8' : return BitContainer {1,0,0,0};
      case '9' : return BitContainer {1,0,0,1};
      case 'A' : return BitContainer {1,0,1,0};
      case 'B' : return BitContainer {1,0,1,1};
      case 'C' : return BitContainer {1,1,0,0};
      case 'D' : return BitContainer {1,1,0,1};
      case 'E' : return BitContainer {1,1,1,0};
      case 'F' : return BitContainer {1,1,1,1};
    }
  };

  // convert a string of hex code into vector of bits.
  auto Hex2BinCode = [&Hex2Bin] ( const std::string & str ) -> BitContainer {
    BitContainer output;
    for ( const char & c : str ) {
      BitContainer nymble = Hex2Bin ( c );
      for ( bool b : nymble ) output . push_back ( b );
    }
    return output;
  };

  // convert a vector of bits into a string of hex code
  auto Bin2HexCode = [] ( const BitContainer & bin ) -> std::string {
    uint64_t nymbleSize = 4;
    typedef std::bitset<4> Nymble;
    BitContainer::const_iterator it;
    std::stringstream res;
    for ( it = bin.begin(); it!=bin.end(); it+=nymbleSize ) {
      Nymble nymble;
      nymble[3] = *it;
      nymble[2] = *(it+1);
      nymble[1] = *(it+2);
      nymble[0] = *(it+3);
      res << std::hex << std::uppercase << nymble.to_ulong();
    }
    return res.str();
  };

  /// DEBUG hex/bin conversion
  // std::string testString = "0F9ABD12146C";
  // BitContainer testBool = Hex2BinCode ( testString );
  // for ( bitType b : testBool ) std::cout << b << " ";
  // std::cout << "\n";
  // std::string s = Bin2HexCode ( Hex2BinCode ( testString ) );
  // std::cout << testString << "\n";
  // std::cout << s << "\n";
  /// END DEBUG

  std::set<uint64_t> outputSet; // to have unicity
  // Will do in-place bit flip to avoid temporary copies
  for ( uint64_t d = 0; d < D; ++d ) {
    /// make a copy of the Logic Parameter
    LogicParameter lp = logics [ d ];
    /// extract the hexcode of the logic parameter
    std::string hexCode ( lp . hex ( ) );
    /// Convert the hexCode into binary code
    BitContainer binCode ( Hex2BinCode ( hexCode ) );
    /// Flip one bit at a time, construct a new adjacent parameter
    /// and retrieve its index
    uint64_t N = binCode . size ( );
    for ( uint64_t i = 0; i < N; ++i ) {
      binCode[i] = 1-binCode[i]; // flip
      //
      // new logic from the flipped binCode
      logics [ d ] = LogicParameter (
                            data_ -> network_ . inputs ( d ) . size(),
                            data_ -> network_ . outputs ( d ) . size(),
                            Bin2HexCode ( binCode )
                                    );
      //
      Parameter adj_p ( logics,
                        orders, /// Don't change the order of reference parameter?
                        data_ -> network_ );
      //
      outputSet . insert ( ParameterGraph::index(adj_p) );
      //
      // undo the modifications
      binCode[i] = 1-binCode[i]; // flip it back
      logics [ d ] = lp;
    }
  }

  for ( auto i : outputSet ) { output . push_back ( i ); }

  return output;

}

INLINE_IF_HEADER_ONLY Network const ParameterGraph::
network ( void ) const {
  return data_ -> network_;
}

INLINE_IF_HEADER_ONLY std::ostream& operator << ( std::ostream& stream, ParameterGraph const& pg ) {
  stream <<  "(ParameterGraph: "<< pg.size() << " parameters, "
         << pg.network().size() << " nodes)";
  return stream;
}

INLINE_IF_HEADER_ONLY uint64_t ParameterGraph::
fixedordersize ( void ) const {
  return data_ -> fixedordersize_;
}

INLINE_IF_HEADER_ONLY uint64_t ParameterGraph::
reorderings ( void ) const {
  return data_ -> reorderings_;
}

INLINE_IF_HEADER_ONLY uint64_t ParameterGraph::
_factorial ( uint64_t m ) const {
  static const std::vector<uint64_t> table =
    { 1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880};
  if ( m < 10 ) return table [ m ]; else return m * _factorial ( m - 1 );
}


#endif
