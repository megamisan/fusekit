
#ifndef __FUSEKIT__DAEMON_H_
#define __FUSEKIT__DAEMON_H_

#ifndef FUSE_USE_VERSION
#pragma message "FUSE_USE_VERSION is not defined. setting FUSE_USE_VERSION=27"
#define FUSE_USE_VERSION 27
#endif

#include <unistd.h>
#include <sys/types.h>
#include <fuse/fuse.h>
#include <vector>
#include <sstream>

#include <fusekit/entry.h>
#include <fusekit/file_handle.h>
#include <fusekit/no_entry.h>
#include <fusekit/no_lock.h>
#include <fusekit/default_directory.h>
#include <fusekit/path.h>

namespace fusekit{

  /// daemon singleton which implements the fuse_operations
  /// interface and delegates the file operations to file hierarchy entries.
  ///
  /// all file operations are delegated to the respective file hierarchy
  /// element which implements the entry interface. for all methods (operation) 
  /// the daemon processes the input argument "path" to find out the right
  /// entry, which will actually handle the job. in addition the daemon itself 
  /// contains the root element of the filesystem. in most cases this element will
  /// model some kind of directory. 
  template< 
    class Root = typename fusekit::default_directory<>::type, 
    class LockingPolicy = fusekit::no_lock 
    >
  struct daemon : public LockingPolicy {
    static daemon& instance() {
      static daemon d;
      return d;
    }

    typedef typename daemon< Root, LockingPolicy >::lock lock;

    Root& root(){
      return _root;
    }

    /// runs / starts / mounts the filesystem daemon and returns after
    /// filesystem has been unmounted.
    ///
    /// if default_options is true, the filesystem will only handle
    /// one fileoperation at a time and the default permission / access
    /// handling of fuse is used, based on the mode flags given by
    /// the getattr call.
    /// technically default_options == true expands the given command line
    /// (argc,argv) with "-s -o default_permissions".
    int run( int argc, char* argv[], bool default_options = true ){
      std::vector< char* > argv_vec(argv,argv+argc);
      if( default_options ){
	update_uid();
	update_gid();
	argv_vec.push_back(const_cast< char* >("-s"));
	argv_vec.push_back(const_cast< char* >("-o"));
	argv_vec.push_back(const_cast< char* >("default_permissions"));
	argv_vec.push_back(const_cast< char* >("-o"));
	argv_vec.push_back(const_cast< char* >(_uid.c_str()));
	argv_vec.push_back(const_cast< char* >("-o"));
	argv_vec.push_back(const_cast< char* >(_gid.c_str()));
      }
#if FUSE_USE_VERSION > 26
      return fuse_main( argv_vec.size(), &argv_vec[0], &_ops, NULL );
#else
      return fuse_main( argv_vec.size(), &argv_vec[0], &_ops );
#endif
    }

  protected:
    /**
     * \brief Add handlers for global operations.
     * \remark Run before setting internal handlers.
     */
    virtual void extendOperations(fuse_operations &ops) {}

  private:

    daemon(){
      extendOperations(_ops);
      _ops.getattr = daemon::getattr;
      _ops.readlink = daemon::readlink;
      _ops.opendir = daemon::opendir;
      _ops.readdir = daemon::readdir;
      _ops.releasedir = daemon::releasedir;
      _ops.read = daemon::read;
      _ops.write = daemon::write;
      _ops.truncate = daemon::truncate;
      _ops.open = daemon::open;
      _ops.release = daemon::release;
      _ops.chmod = daemon::chmod;
      _ops.mknod = daemon::mknod;
      _ops.unlink = daemon::unlink;
      _ops.mkdir = daemon::mkdir;
      _ops.rmdir = daemon::rmdir;
      _ops.symlink = daemon::symlink;
      _ops.flush = daemon::flush;
      _ops.setxattr = daemon::setxattr;
      _ops.getxattr = daemon::getxattr;
      _ops.listxattr = daemon::listxattr;
      _ops.removexattr = daemon::removexattr;
#if FUSE_USE_VERSION > 24
      _ops.access  = daemon::access;
#endif
#if FUSE_USE_VERSION > 25
      _ops.utimens = daemon::utimens;
#else
      _ops.utime = daemon::utime;
#endif
    }

    static int unlink( const char* p ){
      lock guard(instance());
      path parent(p);
      const std::string to_delete = parent.back();
      parent.pop_back();
      return instance().find_entry(parent).unlink(to_delete.c_str());
    }

    static int mknod( const char* p, mode_t m, dev_t t ){
      lock guard(instance());
      path parent(p);
      const std::string to_create = parent.back();
      parent.pop_back();
      return instance().find_entry(parent).mknod(to_create.c_str(), m, t);
    }

    static int mkdir( const char* p, mode_t m ){
      lock guard(instance());
      path parent(p);
      const std::string to_create = parent.back();
      parent.pop_back();
      return instance().find_entry(parent).mkdir(to_create.c_str(), m);
    }

    static int rmdir( const char* p ){
      lock guard(instance());
      path parent(p);
      const std::string to_create = parent.back();
      parent.pop_back();
      return instance().find_entry(parent).rmdir(to_create.c_str());
    }

    static int access( const char* path, int perm ){
      lock guard(instance());
      return instance().find_entry(path).access(perm);
    }

    static int chmod( const char* path, mode_t perm ){
      lock guard(instance());
      return instance().find_entry(path).chmod(perm);
    }

    static int open( const char* path, struct fuse_file_info* fi ){
      lock guard(instance());
      return instance().find_entry(path).open(*fi);
    }

    static int release( const char* path, struct fuse_file_info* fi ){
      lock guard(instance());
      int err = instance().find_entry(path).release(*fi);
      if( err == -ENOENT && fi->fh ){
	// close has been called on a file, which is no more 
	delete reinterpret_cast< file_handle* >(fi->fh);
	fi->fh = 0;
      }
      return err;
    }

    static int flush( const char* path, struct fuse_file_info* fi ){
      lock guard(instance());
      return instance().find_entry(path).flush(*fi);
    }

    static int truncate( const char* path, off_t offset ){
      lock guard(instance());
      return instance().find_entry(path).truncate( offset );
    }

    static int getattr( const char* path, struct stat* stbuf ){
      lock guard(instance());
      return instance().find_entry(path).stat(*stbuf);
    }

    static int read( const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi ){
      lock guard(instance());
      return instance().find_entry(path).read(buf,size,offset,*fi);
    }

    static int write( const char* path, const char* src, size_t size, off_t offset, struct fuse_file_info* fi ){
      lock guard(instance());
      return instance().find_entry(path).write(src,size,offset,*fi);
    }

    static int opendir( const char *path, struct fuse_file_info *fi ){
      lock guard(instance());
      return instance().find_entry(path).opendir(*fi);
    }

    static int readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ){
      lock guard(instance());
      return instance().find_entry(path).readdir(buf,filler,offset,*fi);
    }

    static int releasedir( const char *path, struct fuse_file_info *fi ){
      lock guard(instance());
      return instance().find_entry(path).releasedir(*fi);
    }

    static int utime( const char *path, utimbuf* buf ){
      lock guard(instance());
      struct timespec tv[2] = { 0 };
      tv[0].tv_sec = buf->actime;
      tv[1].tv_sec = buf->modtime;
      return instance().find_entry(path).utimens(tv);
    }

    static int utimens( const char *path, const struct timespec tv[2] ){
      lock guard(instance());
      return instance().find_entry(path).utimens(tv);
    }

    static int readlink( const char *path, char *buffer, size_t size ){
      lock guard(instance());
      return instance().find_entry(path).readlink(buffer, size);
    }

    static int symlink( const char *path, const char* target ){
      lock guard(instance());
      struct path pa = path;
      const std::string name = pa.back();
      pa.pop_back();
      return instance().find_entry(pa).symlink(name.c_str(), target);
    }

    static int setxattr( const char *path, const char *name, const char *value, size_t size, int flags ){
      lock guard(instance());
      return instance().find_entry(path).setxattr(name, value, size, flags);
    }

    static int getxattr( const char *path, const char *name, char *value, size_t size ){
      lock guard(instance());
      return instance().find_entry(path).getxattr(name, value, size);
    }

    static int listxattr( const char *path, char *list, size_t size ){
      lock guard(instance());
      return instance().find_entry(path).listxattr(list, size);
    }

    static int removexattr( const char *path, const char *name ){
      lock guard(instance());
      return instance().find_entry(path).removexattr(name);
    }

    fusekit::entry& find_entry( const path& pa ){
      if( pa.empty() ) {
	return this->_root;
      }
      path::const_iterator p = pa.begin();  
      entry* e = &this->_root;
      do {
	e  = e->child(p->c_str());
	if( !e ) {
	  static no_entry noent;
	  return noent;
	}
      } while( ++p != pa.end() );
      return *e;
    }   

    void update_uid(){
      _uid = "uid=";
      std::ostringstream uid_stream;
      uid_stream << ::getuid();
      _uid += uid_stream.str();
    }

    void update_gid(){
      _gid = "gid=";
      std::ostringstream gid_stream;
      gid_stream << ::getgid();
      _gid += gid_stream.str();
    }
    
    Root _root;
    fuse_operations _ops;
    std::string _uid;
    std::string _gid;
  };
}

#endif



