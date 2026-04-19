#ifndef LEFTRIGHTDATA_H
#define LEFTRIGHTDATA_H

#include <cstring>
#include <vector>

template <typename T> class LeftRight {
public:
  /**
   * @brief Class constructor
   *
   * @param data data to instantiate LeftRight datatype
   */
  LeftRight(std::vector<T> data) {

    // Assign initial pointers to buffers
    length = data.size();
    read_ptr = new std::vector<T>(length);
    write_ptr = new std::vector<T>(length);
    read_ptr->data() = data.data();
    write_ptr->data() = data.data();
  };

  /**
   * @brief Default constructor
   */
  LeftRight(void) {
    length = 1;
    read_ptr = new std::vector<T>(1);
    write_ptr = new std::vector<T>(1);
  }

  /**
   * @brief Class constructor from size
   *
   * @param size data size
   */
  LeftRight(int size) {
    // Assign initial pointers to buffers
    read_ptr = new std::vector<T>(size);
    write_ptr = new std::vector<T>(size);
    length = size;
  };

  /// read all data from reader buffer
  std::vector<T> read(void) { return *read_ptr; };

  /// read specific ID from reader buffer
  T read(int id) { return read_ptr->at(id); };

  // number of entries in data
  int length;

  /**
   * @brief Overwrite entire writer buffer and switch read and writer
   *
   * @param data Entire vector of data to overwrite existing buffer
   */
  void write(std::vector<T> data) {
    std::memcpy(write_ptr, data.data, data.size());
    swap_lr();
  };

  /**
   * @brief Overwrite single index of buffer
   *
   * @param val value to overwrite location id
   * @param id index to overwrite
   */
  void write(T val, int id) { write_ptr->at(id) = val; };

  /// Finish writing and swap buffer
  void swap_lr(void) { std::swap(read_ptr, write_ptr); };

private:
  /// Pointer to reader
  std::vector<T> *read_ptr;

  /// Pointer to writer
  std::vector<T> *write_ptr;
};

#endif
