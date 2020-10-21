// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#ifndef CEPH_RGW_ROLE_H
#define CEPH_RGW_ROLE_H

#include <string>

#include "common/async/yield_context.h"

#include "common/ceph_json.h"
#include "common/ceph_context.h"
#include "rgw/rgw_rados.h"
#include "rgw_metadata.h"

class RGWCtl;
class RGWRados;
class RGWRoleMetadataHandler;
class RGWSI_Role;
class RGWSI_MetaBackend_Handler;
class RGWRoleCtl;

namespace rgw { namespace sal {
class RGWRole
{
public:
  static const std::string role_name_oid_prefix;
  static const std::string role_oid_prefix;
  static const std::string role_path_oid_prefix;
  static const std::string role_arn_prefix;
  static constexpr int MAX_ROLE_NAME_LEN = 64;
  static constexpr int MAX_PATH_NAME_LEN = 512;
  static constexpr uint64_t SESSION_DURATION_MIN = 3600; // in seconds
  static constexpr uint64_t SESSION_DURATION_MAX = 43200; // in seconds
protected:

  RGWRoleCtl *role_ctl;

  std::string id;
  std::string name;
  std::string path;
  std::string arn;
  std::string creation_date;
  std::string trust_policy;
  std::map<std::string, std::string> perm_policy_map;
  std::string tenant;
  uint64_t max_session_duration;
  std::multimap<std::string,std::string> tags;

public:
  virtual int store_info(const DoutPrefixProvider *dpp, bool exclusive, optional_yield y);
  virtual int store_name(const DoutPrefixProvider *dpp, bool exclusive, optional_yield y) { return 0; }
  virtual int store_path(const DoutPrefixProvider *dpp, bool exclusive, optional_yield y) { return 0; }
  virtual int read_id(const DoutPrefixProvider *dpp, const std::string& role_name, const std::string& tenant, std::string& role_id, optional_yield y);
  virtual int read_name(const DoutPrefixProvider *dpp, optional_yield y);
  virtual int read_info(const DoutPrefixProvider *dpp, optional_yield y);
  bool validate_input(const DoutPrefixProvider* dpp);
  void extract_name_tenant(const std::string& str);

  RGWRole(std::string name,
          std::string tenant,
          std::string path="",
          std::string trust_policy="",
          std::string max_session_duration_str="",
          std::multimap<std::string,std::string> tags={})
  : name(std::move(name)),
    path(std::move(path)),
    trust_policy(std::move(trust_policy)),
    tenant(std::move(tenant)),
    tags(std::move(tags)) {
    if (this->path.empty())
      this->path = "/";
    extract_name_tenant(name);
    if (max_session_duration_str.empty()) {
      max_session_duration = SESSION_DURATION_MIN;
    } else {
      max_session_duration = std::stoull(max_session_duration_str);
    }
  }

  RGWRole(std::string id) : id(std::move(id)) {}

  RGWRole() = default;

  virtual ~RGWRole() = default;

  void encode(bufferlist& bl) const {
    ENCODE_START(3, 1, bl);
    encode(id, bl);
    encode(name, bl);
    encode(path, bl);
    encode(arn, bl);
    encode(creation_date, bl);
    encode(trust_policy, bl);
    encode(perm_policy_map, bl);
    encode(tenant, bl);
    encode(max_session_duration, bl);
    ENCODE_FINISH(bl);
  }

  void decode(bufferlist::const_iterator& bl) {
    DECODE_START(3, bl);
    decode(id, bl);
    decode(name, bl);
    decode(path, bl);
    decode(arn, bl);
    decode(creation_date, bl);
    decode(trust_policy, bl);
    decode(perm_policy_map, bl);
    if (struct_v >= 2) {
      decode(tenant, bl);
    }
    if (struct_v >= 3) {
      decode(max_session_duration, bl);
    }
    DECODE_FINISH(bl);
  }

  const std::string& get_id() const { return id; }
  const std::string& get_name() const { return name; }
  const std::string& get_tenant() const { return tenant; }
  const std::string& get_path() const { return path; }
  const std::string& get_create_date() const { return creation_date; }
  const std::string& get_assume_role_policy() const { return trust_policy;}
  const uint64_t& get_max_session_duration() const { return max_session_duration; }

  void set_id(const std::string& id) { this->id = id; }
  void set_arn(const std::string& arn) { this->arn = arn; }
  void set_creation_date(const std::string& creation_date) { this->creation_date = creation_date; }

  virtual int create(const DoutPrefixProvider *dpp, bool exclusive, optional_yield y);
  virtual int delete_obj(const DoutPrefixProvider *dpp, optional_yield y);
  int get(const DoutPrefixProvider *dpp, optional_yield y);
  int get_by_id(const DoutPrefixProvider *dpp, optional_yield y);
  int update(const DoutPrefixProvider *dpp, optional_yield y);
  void update_trust_policy(std::string& trust_policy);
  void set_perm_policy(const std::string& policy_name, const std::string& perm_policy);
  std::vector<std::string> get_role_policy_names();
  int get_role_policy(const DoutPrefixProvider* dpp, const std::string& policy_name, std::string& perm_policy);
  int delete_policy(const DoutPrefixProvider* dpp, const std::string& policy_name);
  int set_tags(const DoutPrefixProvider* dpp, const std::multimap<std::string,std::string>& tags_map);
  boost::optional<std::multimap<std::string,std::string>> get_tags();
  void erase_tags(const std::vector<std::string>& tagKeys);
  void dump(Formatter *f) const;
  void decode_json(JSONObj *obj);

  static const std::string& get_names_oid_prefix();
  static const std::string& get_info_oid_prefix();
  static const std::string& get_path_oid_prefix();
};
WRITE_CLASS_ENCODER(RGWRole)
} } // namespace rgw::sal

struct RGWRoleCompleteInfo {
  rgw::sal::RGWRole info;
  std::map<std::string, bufferlist> attrs;
  bool has_attrs;

  void dump(Formatter *f) const;
  void decode_json(JSONObj *obj);
};

class RGWRoleMetadataObject: public RGWMetadataObject {
  RGWRoleCompleteInfo rci;
public:
  RGWRoleMetadataObject() = default;
  RGWRoleMetadataObject(const RGWRoleCompleteInfo& _rci,
			const obj_version& v,
			real_time m) : RGWMetadataObject(v,m), rci(_rci) {}

  void dump(Formatter *f) const override {
    rci.dump(f);
  }

  RGWRoleCompleteInfo& get_rci() {
    return rci;
  }
};
//class RGWMetadataObject;

class RGWRoleMetadataHandler: public RGWMetadataHandler_GenericMetaBE
{

public:
  struct Svc {
    RGWSI_Role *role{nullptr};
  } svc;

  RGWRoleMetadataHandler(RGWSI_Role *role_svc);

  std::string get_type() final { return "roles";  }

  RGWMetadataObject *get_meta_obj(JSONObj *jo,
				  const obj_version& objv,
				  const ceph::real_time& mtime);

  int do_get(RGWSI_MetaBackend_Handler::Op *op,
	     std::string& entry,
	     RGWMetadataObject **obj,
	     optional_yield y,
       const DoutPrefixProvider *dpp) final;

  int do_remove(RGWSI_MetaBackend_Handler::Op *op,
		std::string& entry,
		RGWObjVersionTracker& objv_tracker,
		optional_yield y,
    const DoutPrefixProvider *dpp) final;

  int do_put(RGWSI_MetaBackend_Handler::Op *op,
	     std::string& entr,
	     RGWMetadataObject *obj,
	     RGWObjVersionTracker& objv_tracker,
	     optional_yield y,
       const DoutPrefixProvider *dpp,
	     RGWMDLogSyncType type,
       bool from_remote_zone) override;
};

class RGWRoleCtl {
  struct Svc {
    RGWSI_Role *role {nullptr};
  } svc;
  RGWRoleMetadataHandler *rmhandler;
  RGWSI_MetaBackend_Handler *be_handler{nullptr};
public:
  RGWRoleCtl(RGWSI_Role *_role_svc,
	     RGWRoleMetadataHandler *_rmhandler) {
    svc.role = _role_svc;
    rmhandler = _rmhandler;
    be_handler = _rmhandler->get_be_handler();
  }

  struct PutParams {
    ceph::real_time mtime;
    bool exclusive {false};
    RGWObjVersionTracker *objv_tracker {nullptr};
    std::map<std::string, bufferlist> *attrs {nullptr};

    PutParams() {}

    PutParams& set_objv_tracker(RGWObjVersionTracker *_objv_tracker) {
      objv_tracker = _objv_tracker;
      return *this;
    }

    PutParams& set_mtime(const ceph::real_time& _mtime) {
      mtime = _mtime;
      return *this;
    }

    PutParams& set_exclusive(bool _exclusive) {
      exclusive = _exclusive;
      return *this;
    }

    PutParams& set_attrs(std::map<std::string, bufferlist> *_attrs) {
      attrs = _attrs;
      return *this;
    }
  };

  struct GetParams {
    ceph::real_time *mtime{nullptr};
    std::map<std::string, bufferlist> *attrs{nullptr};
    RGWObjVersionTracker *objv_tracker {nullptr};

    GetParams() {}

    GetParams& set_objv_tracker(RGWObjVersionTracker *_objv_tracker) {
      objv_tracker = _objv_tracker;
      return *this;
    }

    GetParams& set_mtime(ceph::real_time *_mtime) {
      mtime = _mtime;
      return *this;
    }

    GetParams& set_attrs(std::map<std::string, bufferlist> *_attrs) {
      attrs = _attrs;
      return *this;
    }
  };

  struct RemoveParams {
    RGWObjVersionTracker *objv_tracker{nullptr};

    RemoveParams() {}

    RemoveParams& set_objv_tracker(RGWObjVersionTracker *_objv_tracker) {
      objv_tracker = _objv_tracker;
      return *this;
    }
  };


  int create(rgw::sal::RGWRole& role,
	     optional_yield y,
       const DoutPrefixProvider *dpp,
	     const PutParams& params = {});

  int store_info(const rgw::sal::RGWRole& role,
		 optional_yield y,
     const DoutPrefixProvider *dpp,
		 const PutParams& params = {});

  // The methods for store name & store path are currently unused and only
  // useful for a potential rename name/path functionality in the future as
  // create role would automatically create these for most uses
  int store_name(const std::string& role_id,
		 const std::string& name,
		 const std::string& tenant,
		 optional_yield y,
     const DoutPrefixProvider *dpp,
		 const PutParams& params = {});

  int store_path(const std::string& role_id,
		 const std::string& path,
		 const std::string& tenant,
		 optional_yield y,
     const DoutPrefixProvider *dpp,
		 const PutParams& params = {});

  int read_info(const std::string& role_id,
					optional_yield y,
          const DoutPrefixProvider *dpp,
          rgw::sal::RGWRole* role,
					const GetParams& params = {});

  std::pair<int, std::string> read_name(const std::string& name,
				   const std::string& tenant,
				   optional_yield y,
           const DoutPrefixProvider *dpp,
				   const GetParams& params = {});

  int delete_info(const std::string& role_id,
		  optional_yield y,
      const DoutPrefixProvider *dpp,
		  const RemoveParams& params = {});

  int delete_name(const std::string& name,
		  const std::string& tenant,
		  optional_yield y,
      const DoutPrefixProvider *dpp,
		  const RemoveParams& params = {});

  int delete_path(const std::string& role_id,
		  const std::string& path,
		  const std::string& tenant,
		  optional_yield y,
      const DoutPrefixProvider *dpp,
		  const RemoveParams& params = {});
};
#endif /* CEPH_RGW_ROLE_H */
