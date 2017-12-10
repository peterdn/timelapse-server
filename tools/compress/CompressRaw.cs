using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

namespace CompressRaw
{
    class Program
    {
        static void Main(string[] args)
        {
            var rawFilePath = args[0];
            var compressedFilePath = args[1];

            Bitmap rawBmp = null;
            Bitmap compressedBmp = null;
            try
            {
                Console.WriteLine("Compressing...");
                var t = Environment.TickCount;
                rawBmp = Compress(rawFilePath, compressedFilePath);
                Console.WriteLine("Took {0}ms", Environment.TickCount - t);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Failed to compress!");
                Console.WriteLine(ex.ToString());
                Environment.Exit(-1);
            }

            // Perform verification
            byte[] decompressedBuffer = null;
            try
            {
                Console.WriteLine("Decompressing to verify...");
                var t = Environment.TickCount;
                compressedBmp = Decompress(compressedFilePath, out decompressedBuffer);
                Console.WriteLine("Took {0}ms", Environment.TickCount - t);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Failed to decompress!");
                Console.WriteLine(ex.ToString());
                Environment.Exit(-1);
            }

            try
            {
                /*Console.WriteLine("Verifying...");
                var t = Environment.TickCount;
                VerifyPixels(rawBmp, compressedBmp);
                VerifyRawData(rawFilePath, decompressedBuffer);
                Console.WriteLine("Took {0}ms", Environment.TickCount - t);*/
            }
            catch (Exception ex)
            {
                Console.WriteLine("Verification failed!");
                Console.WriteLine(ex.ToString());
                Environment.Exit(-1);
            }

            Console.WriteLine("Success.");
        }

        static void VerifyPixels(Bitmap B1, Bitmap B2)
        {
            if (B1.Height != B2.Height || B1.Width != B2.Width)
            {
                throw new InvalidDataException("Bitmaps are different sizes");
            }

            for (var y = 0; y < B1.Height; ++y)
            {
                for (var x = 0; x < B1.Width; ++x)
                {
                    if (B1.GetPixel(x, y) != B2.GetPixel(x, y))
                    {
                        throw new InvalidDataException(string.Format("Pixels at {0},{1} are different: {2} vs {3}",
                                x, y, B1.GetPixel(x, y), B2.GetPixel(x, y)));
                    }
                }
            }
        }


        static void VerifyRawData(string OriginalFilePath, byte[] NewFile)
        {
            var originalFile = File.ReadAllBytes(OriginalFilePath);

            if (originalFile.Length != NewFile.Length)
            {
                throw new InvalidDataException("Files aren't the same length!");
            }

            for (var i = 0; i < originalFile.Length; ++i)
            {
                if (originalFile[i] != NewFile[i])
                {
                    var rawOffset = (i - 32768);
                    var line = rawOffset / 3264;
                    if (line >= 1944)
                    {
                        break;
                    }
                    var lineOffset = rawOffset % 3264;
                    if (lineOffset < 3240 && (lineOffset + 1) % 5 != 0)
                    {
                        throw new InvalidDataException(string.Format("Byte {0} isn't the same: {1} vs {2}",
                                i, originalFile[i], NewFile[i]));
                    }
                }
            }
        }

        static void VerifyHeader(byte[] Header)
        {
            if (Header[0] != 'B' || Header[1] != 'R' || Header[2] != 'C' || Header[3] != 'M')
            {
                throw new InvalidDataException("Invalid header");
            }
        }

        static Bitmap Compress(string InputFileName, string OutputFileName)
        {
            byte[] header = new byte[32768];
            byte[] image;
            using (var f = File.OpenRead(InputFileName))
            {
                // Skip past header
                f.Read(header, 0, header.Length);
                image = new byte[f.Length - header.Length];
                f.Read(image, 0, image.Length);
            }

            VerifyHeader(header);

            var bmp = new Bitmap(2592 / 2, 1944 / 2);
            for (var r = 0; r < 1944 / 2; ++r)
            {
                for (var c = 0; c < 2592 / 2; ++c)
                {
                    // Bayer 2x2 superpixel with top-left corner at sx, sy
                    var sx = c * 2;
                    var sy = r * 2;

                    // Offset of block at:
                    var offset1 = sy * 3264 + sx + c / 2;
                    var offset2 = (sy + 1) * 3264 + sx + c / 2;

                    var green = image[offset1];
                    var blue = image[offset1 + 1];
                    var red = image[offset2];
                    var alpha = image[offset2 + 1];
                    var color = Color.FromArgb(alpha, red, green, blue);
                    bmp.SetPixel(c, r, color);
                }
            }

            using (var writer = new StreamWriter(OutputFileName, false))
            {
                writer.BaseStream.Write(header, 0, header.Length);
                bmp.Save(writer.BaseStream, ImageFormat.Png);
            }

            return bmp;
        }

        static Bitmap Decompress(string InputFileName, out byte[] DecompressedBuffer)
        {
            byte[] header = new byte[32768];
            byte[] image;
            using (var f = File.OpenRead(InputFileName))
            {
                // Skip past header
                f.Read(header, 0, header.Length);
                image = new byte[f.Length - header.Length];
                f.Read(image, 0, image.Length);
            }

            byte[] raw = new byte[6371328];
            var bmp = new Bitmap(new MemoryStream(image));
            for (var r = 0; r < bmp.Height; ++r)
            {
                for (var c = 0; c < bmp.Width; ++c)
                {
                    var pixel = bmp.GetPixel(c, r);

                    // Bayer 2x2 superpixel with top-left corner at sx, sy
                    var sx = c * 2;
                    var sy = r * 2;

                    // Offset of block at:
                    var offset1 = sy * 3264 + sx + c / 2;
                    var offset2 = (sy + 1) * 3264 + sx + c / 2;

                    raw[offset1] = pixel.G;
                    raw[offset1 + 1] = pixel.B;
                    raw[offset2] = pixel.R;
                    raw[offset2 + 1] = pixel.A;
                }
            }

            var ms = new MemoryStream();
            ms.Write(header, 0, header.Length);
            ms.Write(raw, 0, raw.Length);
            DecompressedBuffer = ms.ToArray();
            return bmp;
        }
    }
}

