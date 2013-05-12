﻿using img_detective.ui.model;
using System;
using System.Collections.Generic;
using System.Diagnostics.Contracts;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace img_detective.cswrapper
{
    

    public static class ImgDetectiveLib
    {
        public static bool UploadImg(byte[] imgContent, string fileExtension)
        {
            Contract.Assert(imgContent != null);
            Contract.Assert(imgContent.Length > 0);
            Contract.Assert(!String.IsNullOrWhiteSpace(fileExtension));

            IntPtr unmanagedBuffer = IntPtr.Zero;

            try
            {
                unmanagedBuffer = Marshal.AllocCoTaskMem(imgContent.Length);
                Marshal.Copy(imgContent, 0, unmanagedBuffer, imgContent.Length);
                var rawImg = new RawImg()
                {
                    content = unmanagedBuffer,
                    contentSize = (UInt32)imgContent.Length,
                    fileExtension = fileExtension
                };
                var result = new UploadImgResult();
                UploadImg(rawImg, out result);

                return result.opStatus == 0;
            }
            finally
            {
                if (unmanagedBuffer != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(unmanagedBuffer);
                }
            }
        }

        public static CanIndexDirectoryResult.ResultEnum CanIndexDirectory(string dirPath)
        {
            CanIndexDirectoryResult result = new CanIndexDirectoryResult();
            CanIndexDirectory(dirPath, out result);

            if (result.opStatus != 0 || result.resultCode == CanIndexDirectoryResult.ResultEnum.Unknown)
            {
                throw new Exception("Couldn't check the specified directory. Call to the umanaged DLL failed");
            }

            return result.resultCode;
        }

        public static void IndexDirectory(string dirPath)
        {
            IndexDirectoryResult result = new IndexDirectoryResult();
            IndexDirectory(dirPath, out result);

            if (result.opStatus != 0)
            {
                throw new Exception("Couldn't index the specified directory. Call to the unmanaged DLL failed");
            }
        }

        public static ui.model.SearchResult Search(byte[] imgContent)
        {
            Contract.Requires(imgContent != null);
            Contract.Requires(imgContent.Length != 0);

            IntPtr imgContentUnmanagedBuf = IntPtr.Zero;
            var result = new SearchResult()
            {
                items = IntPtr.Zero,
                itemsRelevance = IntPtr.Zero
            };

            try
            {
                imgContentUnmanagedBuf = Marshal.AllocCoTaskMem(imgContent.Length);
                Marshal.Copy(imgContent, 0, imgContentUnmanagedBuf, imgContent.Length);

                var query = new ImgQuery()
                {
                    exampleContent = imgContentUnmanagedBuf,
                    exampleContentSize = (UInt32)imgContent.Length,
                    tolerance = 0.5
                };

                Search(query, out result);

                if (result.opStatus != 0)
                {
                    throw new Exception("Call to the unmanaged DLL failed. Search cannot be performed");
                }

                if (result.items == IntPtr.Zero || result.itemsRelevance == IntPtr.Zero || result.arraySize == 0)
                {
                    //no images found
                    return new ui.model.SearchResult();
                }

                long[] imgIds = new long[result.arraySize];
                double[] relevances = new double[result.arraySize];
                Marshal.Copy(result.items, imgIds, 0, (int)result.arraySize);
                Marshal.Copy(result.itemsRelevance, relevances, 0, (int)result.arraySize);

                return new ui.model.SearchResult(imgIds, relevances);
            }
            finally
            {
                if (imgContentUnmanagedBuf != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(imgContentUnmanagedBuf);
                }

                if (result.items != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(result.items);
                }

                if (result.itemsRelevance != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(result.itemsRelevance);
                }
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct UploadImgResult
        {
            public Int32 opStatus;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct RawImg
        {
            public UInt32 contentSize;
            public IntPtr content;
            [MarshalAs(UnmanagedType.LPStr)]
            public string fileExtension;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CanIndexDirectoryResult
        {
            public enum ResultEnum
            {
                Unknown = 0,
                AvailableForIndex = 1,
                AlreadyIndexed = 2,
                NotAbsolute = 3,
                NotExists = 4,
                SubdirIndexed = 5,
                IsNotDir = 6
            }

            public ResultEnum resultCode;
            public Int32 opStatus;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct IndexDirectoryResult
        {
            public Int32 opStatus;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct ImgQuery {
            public UInt32 exampleContentSize;
            public IntPtr exampleContent;
            public double tolerance;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct SearchResult
        {
            public Int32 opStatus;
            public UInt32 arraySize;
            public IntPtr items;
            public IntPtr itemsRelevance;
        }

        [DllImport("img-detective.facade.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern void UploadImg(RawImg img, out UploadImgResult result);

        [DllImport("img-detective.facade.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void CanIndexDirectory([MarshalAs(UnmanagedType.LPWStr)]string dirPath, out CanIndexDirectoryResult result);

        [DllImport("img-detective.facade.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void IndexDirectory([MarshalAs(UnmanagedType.LPWStr)]string dirPath, out IndexDirectoryResult result);

        [DllImport("img-detective.facade.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void Search(ImgQuery query, out SearchResult result);

        [DllImport("img-detective.facade.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void Search1(ImgQuery query);
    }
}
