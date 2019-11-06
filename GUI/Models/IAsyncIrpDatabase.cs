﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GUI.Models
{
    public interface IAsyncIrpDatabase
    {
        /// <summary>
        /// Returns all IRPs. 
        /// </summary>
        Task<IEnumerable<Irp>> GetAsync();


        /// <summary>
        /// Returns all IRPs matching a field matching the given pattern. 
        /// </summary>
        Task<IEnumerable<Irp>> GetAsync(string pattern);


        /// <summary>
        /// Returns an IRP from a GUID. 
        /// </summary>
        Task<Irp> GetAsync(Guid id);
    }
}
