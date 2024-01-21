#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>


// Perform Burrows-Wheeler Transform
// Currently using brute force approach
// Could be improved using suffix arrays
std::string bwt(const std::string& text) {
    int n = text.size();
    std::vector<std::string> rotations(n);

    for (int i = 0; i < n; ++i) {
        rotations[i] = text.substr(i) + text.substr(0, i);
    }
    std::sort(rotations.begin(), rotations.end());
    std::string bwt_text;
    for (const auto& r : rotations) {
        bwt_text += r.back();
    }

    return bwt_text;
}


// Perform run-length encoding
std::string runLengthEncoding(const std::string& input) {
    std::string encoded;
    int count = 1;

    for (size_t i = 1; i <= input.length(); ++i) {
        if (i == input.length() || input[i] != input[i - 1]) {
            // Append the current character
            encoded += input[i - 1];
            // Append the count or special encoding
            if (count == 2) {
                encoded += input[i - 1];
            } else if (count >= 3) {
                count -= 3;
                count += 0x80;
                unsigned char byte = count;
                encoded += static_cast<char>(byte);
            }
            // Reset count
            count = 1;
        } else {
            count++;
        }
    }
    return encoded;
}

int main(int argc, char* argv[]) {
    // Check for correct number of command line arguments
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input file> <output file>" << std::endl;
        return 1;
    }

    // Open input file
    std::ifstream input_file(argv[1]);
    if (!input_file.is_open()) {
        std::cerr << "Error: Cannot open input file." << std::endl;
        return 1;
    }

    // Read content from input file
    std::string input_text((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
    input_file.close();

    // Perform BWT transformation
    std::string bwt_result = bwt(input_text);

    // Perform run-length encoding
    std::string encoded = runLengthEncoding(bwt_result);

    // Open output file for writing
    std::ofstream output_file(argv[2]);
    if (!output_file.is_open()) {
        std::cerr << "Error: Cannot open output file." << std::endl;
        return 1;
    }

    // Write the result to output file
    output_file << encoded;
    output_file.close();

    return 0;
}