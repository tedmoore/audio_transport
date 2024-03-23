#include <iostream>
#include <algorithm>
#include "AudioFile.h"

#include "audio_transport/spectral.hpp"
#include "audio_transport/equal_loudness.hpp"
#include "audio_transport/audio_transport.hpp"

double window_size = 0.05; // seconds
unsigned int padding = 7;  // multiplies window size

int main(int argc, char **argv)
{

  if (argc != 4)
  {
    std::cout << "Usage: " << argv[0] << " input_file time_constant_ms output_file"
              << std::endl;
    return 1;
  }

  double time_constant = std::atof(argv[2]) / 100.;
  if (time_constant < 0)
  {
    std::cout << "time constant must be in greater than zero." << std::endl;
    return 1;
  }
  double interpolation_factor = std::exp(-time_constant / window_size);

  // Open the audio files
  AudioFile<double> af;
  af.load(argv[1]);

  std::cout << "Loaded audio file " << argv[1] << std::endl;
  std::cout << "Number of channels: " << af.getNumChannels() << std::endl;
  std::cout << "Sample rate: " << af.getSampleRate() << std::endl;
  std::cout << "Bit depth: " << af.getBitDepth() << std::endl;
  std::cout << "Number of samples: " << af.getNumSamplesPerChannel() << std::endl;

  // Initialize the output audio
  std::vector<std::vector<double>> audio_interpolated(af.getNumChannels());

  // Iterate over the channels
  for (size_t c = 0; c < af.getNumChannels(); c++)
  {

    std::cout << "Processing channel " << c << std::endl;

    std::cout << "Converting input to the spectral domain" << std::endl;
    std::vector<std::vector<audio_transport::spectral::point>> points =
        audio_transport::spectral::analysis(af.samples[c], af.getSampleRate(), window_size, padding);

    std::cout << "Applying equal loudness filter" << std::endl;
    audio_transport::equal_loudness::apply(points);

    // Construct previous points
    std::vector<audio_transport::spectral::point> points_prev(points[0].size());
    for (unsigned int i = 0; i < points_prev.size(); i++)
    {
      points_prev[i] = points[0][i];
    }

    // Initialize phases
    std::vector<double> phases(points[0].size(), 0);

    std::cout << "Performing optimal transport based interpolation" << std::endl;
    size_t num_windows = points.size();
    std::vector<std::vector<audio_transport::spectral::point>> points_interpolated(num_windows);
    for (size_t w = 0; w < num_windows; w++)
    {

      points_interpolated[w] =
          audio_transport::interpolate(
              points_prev,
              points[w],
              phases,
              window_size,
              interpolation_factor);

      // Copy over the previous
      points_prev.resize(points_interpolated[w].size());
      for (unsigned int i = 0; i < points_prev.size(); i++)
      {
        points_prev[i] = points_interpolated[w][i];
      }
    }

    std::cout << "Removing equal loudness filter" << std::endl;
    audio_transport::equal_loudness::remove(points);

    std::cout << "Converting the interpolation to the time domain" << std::endl;
    audio_interpolated[c] =
        audio_transport::spectral::synthesis(points_interpolated, padding);
  }

  // Write the file
  std::cout << "Writing to file " << argv[3] << std::endl;
  AudioFile<double> audioFile;
  audioFile.setAudioBuffer(audio_interpolated);
  audioFile.setSampleRate(af.getSampleRate());
  audioFile.setBitDepth(24);
  audioFile.save(argv[3], AudioFileFormat::Wave);
}
