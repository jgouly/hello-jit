driver:
	clang++ driver.cpp -g `llvm-config --ldflags --cxxflags --libs all` -rdynamic
