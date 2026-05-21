# libscidup

`libscidup` is the C++ library layer of ScidUp, built as separable CMake targets with explicit dependency direction. `ScidUp::Core` is the chess-domain foundation; `ScidUp::Database` builds on it for database storage/querying, while `ScidUp::Eco` and `ScidUp::Spelling` remain separate optional libraries.
