#pragma once

struct PtsVec3 { float x, y, z; };
struct PtsVertex3D { float x, y, z, u, v, nx, ny, nz; };
struct PtsMesh3D { ID3D11Buffer* vertices = nullptr; UINT count = 0; };
struct PtsTexture3D { ID3D11ShaderResourceView* view = nullptr; };
struct PtsMatrix4 { float m[16]{}; };

struct PtsSceneConstants {
    float mvp[16];
    float world[16];
    float lightDirectionIntensity[4];
    float lightColor[4];
    float baseColor[4];
    float parameters[4];
};
static_assert(sizeof(PtsSceneConstants) % 16 == 0);
struct PtsSkyConstants {
    float forward[4];
    float rightAspect[4];
    float upTanHalfFov[4];
    float sunDirectionDaylight[4];
    float atmosphere[4];
};
static_assert(sizeof(PtsSkyConstants) % 16 == 0);

struct PtsD3D11State {
    HWND window = nullptr;
    bool running = false;
    bool frameOpen = false;
    bool vsync = true;
    bool mouseLocked = false;
    int mouseDX = 0;
    int mouseDY = 0;
    float clear[4]{0.025f, 0.035f, 0.06f, 1.0f};
    float cameraPosition[3]{4.0f, 3.0f, 7.0f};
    float cameraTarget[3]{0.0f, 0.0f, 0.0f};
    float fovDegrees = 60.0f;
    float lightDirection[3]{-0.4f, -1.0f, -0.3f};
    float lightColor[3]{1.0f, 0.96f, 0.9f};
    float lightIntensity = 0.9f;
    double deltaSeconds = 1.0 / 60.0;
    std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* renderTarget = nullptr;
    ID3D11Texture2D* depthTexture = nullptr;
    ID3D11DepthStencilView* depthView = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11VertexShader* skyVertexShader = nullptr;
    ID3D11PixelShader* skyPixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11Buffer* constants = nullptr;
    ID3D11Buffer* skyConstants = nullptr;
    ID3D11SamplerState* sampler = nullptr;
    ID3D11BlendState* blend = nullptr;
    ID3D11RasterizerState* rasterizer = nullptr;
    ID3D11DepthStencilState* depthState = nullptr;
    ID3D11DepthStencilState* skyDepthState = nullptr;
    PtsMesh3D cube;
    PtsMesh3D sphere;
    std::vector<PtsMesh3D> models{1};
    std::vector<PtsTexture3D> textures{1};
};

inline PtsD3D11State& ptsD3D11State() { static PtsD3D11State state; return state; }
#define ptsD3D11 (ptsD3D11State())

enum class PtsPhysicsShape { Sphere, Box, Plane };
struct PtsRigidBody3D {
    PtsPhysicsShape shape = PtsPhysicsShape::Sphere;
    PtsVec3 position{};
    PtsVec3 velocity{};
    PtsVec3 force{};
    PtsVec3 halfExtents{0.5f, 0.5f, 0.5f};
    float radius = 0.5f;
    float inverseMass = 1.0f;
    float restitution = 0.2f;
    float friction = 0.65f;
    float linearDamping = 0.04f;
    float impactSpeed = 0.0f;
    int contacts = 0;
    bool active = true;
    bool grounded = false;
};
struct PtsPhysicsState3D {
    std::vector<PtsRigidBody3D> bodies{1};
    PtsVec3 gravity{0.0f, -9.81f, 0.0f};
    float fixedStep = 1.0f / 60.0f;
    float accumulator = 0.0f;
    unsigned int maxSubsteps = 8;
};
struct PtsPhysicsContact3D { std::size_t a = 0, b = 0; PtsVec3 normal{}; float penetration = 0.0f; };
inline PtsPhysicsState3D& ptsPhysics3DState() { static PtsPhysicsState3D state; return state; }
#define ptsPhysics3D (ptsPhysics3DState())

struct PtsAudioClip {
    WAVEFORMATEX format{};
    std::vector<BYTE> samples;
};
struct PtsAudioVoice {
    IXAudio2SourceVoice* voice = nullptr;
    std::shared_ptr<PtsAudioClip> clip;
};
struct PtsAudioState {
    HMODULE module = nullptr;
    IXAudio2* engine = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    std::vector<PtsAudioVoice> voices;
    std::unordered_map<std::wstring, std::shared_ptr<PtsAudioClip>> clips;
    PtsVec3 listenerPosition{};
    PtsVec3 listenerForward{0.0f, 0.0f, 1.0f};
    float masterVolume = 1.0f;
    bool attempted = false;
};
inline PtsAudioState& ptsAudio3DState() { static PtsAudioState state; return state; }
#define ptsAudio3D (ptsAudio3DState())
static void ptsAudioShutdown();

std::filesystem::path ptsResolveAssetPath(const std::string& filename) {
    std::filesystem::path path(wide(filename));
    if (path.is_absolute() || std::filesystem::exists(path)) return path;
    std::wstring executable(32768,L'\0');
    const DWORD length=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if(length>0 && length<executable.size()) {
        executable.resize(length);
        const auto beside=std::filesystem::path(executable).parent_path()/path;
        if(std::filesystem::exists(beside))return beside;
    }
    return path;
}

template <typename T> void ptsRelease(T*& item) {
    if (item) { item->Release(); item = nullptr; }
}

PtsVec3 ptsAdd(PtsVec3 a, PtsVec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
PtsVec3 ptsSub(PtsVec3 a, PtsVec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
PtsVec3 ptsMul(PtsVec3 a, float value) { return {a.x * value, a.y * value, a.z * value}; }
float ptsDot(PtsVec3 a, PtsVec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
PtsVec3 ptsCross(PtsVec3 a, PtsVec3 b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
PtsVec3 ptsNormalize(PtsVec3 v) {
    const float length = std::sqrt(std::max(1.0e-12f, ptsDot(v, v)));
    return ptsMul(v, 1.0f / length);
}

static void ptsSetGrounded(PtsRigidBody3D& body, PtsVec3 normalForBody) {
    if (normalForBody.y > 0.45f) body.grounded = true;
}
static void ptsResolveContact(PtsPhysicsState3D& world, const PtsPhysicsContact3D& contact) {
    auto& a = world.bodies[contact.a];
    auto& b = world.bodies[contact.b];
    const float invSum = a.inverseMass + b.inverseMass;
    if (invSum <= 0.0f) return;
    ptsSetGrounded(a, ptsMul(contact.normal, -1.0f));
    ptsSetGrounded(b, contact.normal);
    const PtsVec3 relativeVelocity = ptsSub(b.velocity, a.velocity);
    const float alongNormal = ptsDot(relativeVelocity, contact.normal);
    if (alongNormal < 0.0f) {
        const float restitution = std::min(a.restitution, b.restitution);
        const float impulseMagnitude = -(1.0f + restitution) * alongNormal / invSum;
        const PtsVec3 impulse = ptsMul(contact.normal, impulseMagnitude);
        a.velocity = ptsSub(a.velocity, ptsMul(impulse, a.inverseMass));
        b.velocity = ptsAdd(b.velocity, ptsMul(impulse, b.inverseMass));
        const PtsVec3 afterRelative = ptsSub(b.velocity, a.velocity);
        PtsVec3 tangent = ptsSub(afterRelative, ptsMul(contact.normal, ptsDot(afterRelative, contact.normal)));
        const float tangentLength = std::sqrt(std::max(0.0f, ptsDot(tangent, tangent)));
        if (tangentLength > 1.0e-5f) {
            tangent = ptsMul(tangent, 1.0f / tangentLength);
            float frictionImpulse = -ptsDot(afterRelative, tangent) / invSum;
            const float coefficient = std::sqrt(std::max(0.0f, a.friction * b.friction));
            const float maxFriction = impulseMagnitude * coefficient;
            frictionImpulse = std::clamp(frictionImpulse, -maxFriction, maxFriction);
            const PtsVec3 friction = ptsMul(tangent, frictionImpulse);
            a.velocity = ptsSub(a.velocity, ptsMul(friction, a.inverseMass));
            b.velocity = ptsAdd(b.velocity, ptsMul(friction, b.inverseMass));
        }
        const float impact = -alongNormal;
        a.impactSpeed = std::max(a.impactSpeed, impact);
        b.impactSpeed = std::max(b.impactSpeed, impact);
    }
    constexpr float slop = 0.001f;
    constexpr float correctionPercent = 0.82f;
    const float correctionDistance = std::max(contact.penetration - slop, 0.0f) * correctionPercent / invSum;
    const PtsVec3 correction = ptsMul(contact.normal, correctionDistance);
    a.position = ptsSub(a.position, ptsMul(correction, a.inverseMass));
    b.position = ptsAdd(b.position, ptsMul(correction, b.inverseMass));
}

static bool ptsPhysicsContact(const PtsRigidBody3D& a, const PtsRigidBody3D& b, std::size_t ai,
                             std::size_t bi, PtsPhysicsContact3D& out) {
    if (a.shape == PtsPhysicsShape::Plane && b.shape == PtsPhysicsShape::Plane) return false;
    if (a.shape == PtsPhysicsShape::Plane || b.shape == PtsPhysicsShape::Plane) {
        const bool planeIsA = a.shape == PtsPhysicsShape::Plane;
        const auto& plane = planeIsA ? a : b;
        const auto& body = planeIsA ? b : a;
        float penetration = 0.0f;
        if (body.shape == PtsPhysicsShape::Sphere) penetration = plane.position.y + body.radius - body.position.y;
        else penetration = plane.position.y + body.halfExtents.y - body.position.y;
        if (penetration <= 0.0f) return false;
        out = {ai, bi, planeIsA ? PtsVec3{0, 1, 0} : PtsVec3{0, -1, 0}, penetration};
        return true;
    }
    if (a.shape == PtsPhysicsShape::Sphere && b.shape == PtsPhysicsShape::Sphere) {
        const PtsVec3 delta = ptsSub(b.position, a.position);
        const float distanceSquared = ptsDot(delta, delta);
        const float reach = a.radius + b.radius;
        if (distanceSquared >= reach * reach) return false;
        const float distance = std::sqrt(std::max(distanceSquared, 0.0f));
        out = {ai, bi, distance > 1.0e-5f ? ptsMul(delta, 1.0f / distance) : PtsVec3{1, 0, 0}, reach - distance};
        return true;
    }
    if (a.shape == PtsPhysicsShape::Box && b.shape == PtsPhysicsShape::Box) {
        const PtsVec3 delta = ptsSub(b.position, a.position);
        const float ox = a.halfExtents.x + b.halfExtents.x - std::abs(delta.x);
        const float oy = a.halfExtents.y + b.halfExtents.y - std::abs(delta.y);
        const float oz = a.halfExtents.z + b.halfExtents.z - std::abs(delta.z);
        if (ox <= 0.0f || oy <= 0.0f || oz <= 0.0f) return false;
        if (ox <= oy && ox <= oz) out = {ai, bi, {delta.x < 0.0f ? -1.0f : 1.0f, 0, 0}, ox};
        else if (oy <= oz) out = {ai, bi, {0, delta.y < 0.0f ? -1.0f : 1.0f, 0}, oy};
        else out = {ai, bi, {0, 0, delta.z < 0.0f ? -1.0f : 1.0f}, oz};
        return true;
    }

    const bool sphereIsA = a.shape == PtsPhysicsShape::Sphere;
    const auto& sphere = sphereIsA ? a : b;
    const auto& box = sphereIsA ? b : a;
    const PtsVec3 minimum{box.position.x - box.halfExtents.x, box.position.y - box.halfExtents.y, box.position.z - box.halfExtents.z};
    const PtsVec3 maximum{box.position.x + box.halfExtents.x, box.position.y + box.halfExtents.y, box.position.z + box.halfExtents.z};
    const PtsVec3 closest{std::clamp(sphere.position.x, minimum.x, maximum.x),
                          std::clamp(sphere.position.y, minimum.y, maximum.y),
                          std::clamp(sphere.position.z, minimum.z, maximum.z)};
    const PtsVec3 delta = ptsSub(closest, sphere.position);
    const float distanceSquared = ptsDot(delta, delta);
    if (distanceSquared > sphere.radius * sphere.radius) return false;
    PtsVec3 sphereToBox{};
    float penetration = 0.0f;
    if (distanceSquared > 1.0e-8f) {
        const float distance = std::sqrt(distanceSquared);
        sphereToBox = ptsMul(delta, 1.0f / distance);
        penetration = sphere.radius - distance;
    } else {
        const float faces[6] = {sphere.position.x - minimum.x, maximum.x - sphere.position.x,
                                sphere.position.y - minimum.y, maximum.y - sphere.position.y,
                                sphere.position.z - minimum.z, maximum.z - sphere.position.z};
        const PtsVec3 outward[6] = {{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
        int nearest = 0;
        for (int i = 1; i < 6; ++i) if (faces[i] < faces[nearest]) nearest = i;
        sphereToBox = ptsMul(outward[nearest], -1.0f);
        penetration = sphere.radius + faces[nearest];
    }
    out = {ai, bi, sphereIsA ? sphereToBox : ptsMul(sphereToBox, -1.0f), penetration};
    return true;
}

static void ptsPhysicsSubstep(float dt) {
    auto& world = ptsPhysics3D;
    std::vector<PtsPhysicsContact3D> contacts;
    for (std::size_t i = 1; i < world.bodies.size(); ++i) {
        auto& body = world.bodies[i];
        if (!body.active || body.inverseMass <= 0.0f) continue;
        const PtsVec3 acceleration = ptsAdd(world.gravity, ptsMul(body.force, body.inverseMass));
        body.velocity = ptsAdd(body.velocity, ptsMul(acceleration, dt));
        body.position = ptsAdd(body.position, ptsMul(body.velocity, dt));
        body.velocity = ptsMul(body.velocity, std::exp(-std::max(0.0f, body.linearDamping) * dt));
    }
    for (std::size_t i = 1; i < world.bodies.size(); ++i) {
        auto& a = world.bodies[i];
        if (!a.active) continue;
        for (std::size_t j = i + 1; j < world.bodies.size(); ++j) {
            auto& b = world.bodies[j];
            if (!b.active || a.inverseMass + b.inverseMass <= 0.0f) continue;
            PtsPhysicsContact3D contact{};
            if (ptsPhysicsContact(a, b, i, j, contact)) {
                ++a.contacts; ++b.contacts;
                contacts.push_back(contact);
            }
        }
    }
    for (int iteration = 0; iteration < 4; ++iteration)
        for (const auto& contact : contacts) ptsResolveContact(world, contact);
}

static void* ptsPhysics3DCall(const std::string& name, void** args, int argc) {
    auto number = [&](int index) { return static_cast<float>(numberOf(argument(args, index))); };
    auto bodyAt = [&](int index) -> PtsRigidBody3D* {
        const int id = argumentInt(args, index);
        return id > 0 && static_cast<std::size_t>(id) < ptsPhysics3D.bodies.size() && ptsPhysics3D.bodies[static_cast<std::size_t>(id)].active
            ? &ptsPhysics3D.bodies[static_cast<std::size_t>(id)] : nullptr;
    };
    if (name == "physics_reset" && argc == 0) { ptsPhysics3D = PtsPhysicsState3D{}; return nullptr; }
    if (name == "physics_set_gravity" && argc == 3) { ptsPhysics3D.gravity = {number(0), number(1), number(2)}; return nullptr; }
    if (name == "physics_set_fixed_step" && argc == 1) { ptsPhysics3D.fixedStep = std::clamp(number(0), 1.0f/240.0f, 1.0f/20.0f); return nullptr; }
    if (name == "physics_step" && argc == 1) {
        const float dt = std::clamp(number(0), 0.0f, 0.25f);
        ptsPhysics3D.accumulator = std::min(ptsPhysics3D.accumulator + dt, ptsPhysics3D.fixedStep * static_cast<float>(ptsPhysics3D.maxSubsteps));
        unsigned int steps = 0;
        if (ptsPhysics3D.accumulator >= ptsPhysics3D.fixedStep)
            for (auto& body : ptsPhysics3D.bodies) { body.contacts = 0; body.impactSpeed = 0.0f; body.grounded = false; }
        while (ptsPhysics3D.accumulator >= ptsPhysics3D.fixedStep && steps < ptsPhysics3D.maxSubsteps) {
            ptsPhysicsSubstep(ptsPhysics3D.fixedStep);
            ptsPhysics3D.accumulator -= ptsPhysics3D.fixedStep;
            ++steps;
        }
        if (steps > 0) for (auto& body : ptsPhysics3D.bodies) body.force = {};
        return box(static_cast<std::uint64_t>(steps));
    }
    if (name == "physics_add_sphere" && argc == 5) {
        PtsRigidBody3D body{}; body.shape=PtsPhysicsShape::Sphere; body.position={number(0),number(1),number(2)};
        body.radius=std::clamp(number(3),0.01f,10000.0f); const float mass=number(4); body.inverseMass=mass>0.0f?1.0f/std::clamp(mass,0.0001f,1.0e8f):0.0f;
        int id=-1; for(std::size_t i=1;i<ptsPhysics3D.bodies.size();++i)if(!ptsPhysics3D.bodies[i].active){id=static_cast<int>(i);ptsPhysics3D.bodies[i]=body;break;}
        if(id<0&&ptsPhysics3D.bodies.size()<1025){id=static_cast<int>(ptsPhysics3D.bodies.size());ptsPhysics3D.bodies.push_back(body);}
        return box(static_cast<std::int64_t>(id));
    }
    if (name == "physics_add_box" && argc == 7) {
        PtsRigidBody3D body{};body.shape=PtsPhysicsShape::Box;body.position={number(0),number(1),number(2)};
        body.halfExtents={std::clamp(std::abs(number(3))*0.5f,0.01f,10000.0f),std::clamp(std::abs(number(4))*0.5f,0.01f,10000.0f),std::clamp(std::abs(number(5))*0.5f,0.01f,10000.0f)};
        const float mass=number(6);body.inverseMass=mass>0.0f?1.0f/std::clamp(mass,0.0001f,1.0e8f):0.0f;
        int id=-1; for(std::size_t i=1;i<ptsPhysics3D.bodies.size();++i)if(!ptsPhysics3D.bodies[i].active){id=static_cast<int>(i);ptsPhysics3D.bodies[i]=body;break;}
        if(id<0&&ptsPhysics3D.bodies.size()<1025){id=static_cast<int>(ptsPhysics3D.bodies.size());ptsPhysics3D.bodies.push_back(body);}
        return box(static_cast<std::int64_t>(id));
    }
    if (name == "physics_add_plane" && (argc == 1 || argc == 3)) {
        PtsRigidBody3D body{};body.shape=PtsPhysicsShape::Plane;body.position.y=number(0);body.inverseMass=0.0f;
        if(argc==3){body.restitution=std::clamp(number(1),0.0f,1.0f);body.friction=std::clamp(number(2),0.0f,2.0f);}
        int id=-1; for(std::size_t i=1;i<ptsPhysics3D.bodies.size();++i)if(!ptsPhysics3D.bodies[i].active){id=static_cast<int>(i);ptsPhysics3D.bodies[i]=body;break;}
        if(id<0&&ptsPhysics3D.bodies.size()<1025){id=static_cast<int>(ptsPhysics3D.bodies.size());ptsPhysics3D.bodies.push_back(body);}
        return box(static_cast<std::int64_t>(id));
    }
    if ((name == "physics_remove_body") && argc == 1) { if(auto* body=bodyAt(0)){body->active=false;return box(true);}return box(false); }
    if ((name == "physics_apply_force" || name == "physics_apply_impulse") && argc == 4) {
        if(auto* body=bodyAt(0);body&&body->inverseMass>0.0f){const PtsVec3 value{number(1),number(2),number(3)};if(name=="physics_apply_force")body->force=ptsAdd(body->force,value);else body->velocity=ptsAdd(body->velocity,ptsMul(value,body->inverseMass));return box(true);}return box(false);
    }
    if (name == "physics_set_velocity" && argc == 4) { if(auto* body=bodyAt(0);body&&body->inverseMass>0.0f){body->velocity={number(1),number(2),number(3)};return box(true);}return box(false); }
    if (name == "physics_set_position" && argc == 4) { if(auto* body=bodyAt(0)){body->position={number(1),number(2),number(3)};return box(true);}return box(false); }
    if (name == "physics_set_material" && argc == 3) { if(auto* body=bodyAt(0)){body->restitution=std::clamp(number(1),0.0f,1.0f);body->friction=std::clamp(number(2),0.0f,2.0f);return box(true);}return box(false); }
    if (name == "physics_set_damping" && argc == 2) { if(auto* body=bodyAt(0)){body->linearDamping=std::clamp(number(1),0.0f,50.0f);return box(true);}return box(false); }
    if (name.rfind("physics_position_",0)==0 || name.rfind("physics_velocity_",0)==0) {
        if(argc!=1)return nullptr;auto* body=bodyAt(0);if(!body)return nullptr;
        const bool position=name.rfind("physics_position_",0)==0;const char axis=name.back();
        return box(static_cast<double>(axis=='x'?(position?body->position.x:body->velocity.x):axis=='y'?(position?body->position.y:body->velocity.y):(position?body->position.z:body->velocity.z)));
    }
    if (name == "physics_is_grounded" && argc == 1) { if(auto* body=bodyAt(0))return box(body->grounded);return box(false); }
    if (name == "physics_contact_count" && argc == 1) { if(auto* body=bodyAt(0))return box(static_cast<std::int64_t>(body->contacts));return box(std::int64_t{0}); }
    if (name == "physics_impact_speed" && argc == 1) { if(auto* body=bodyAt(0))return box(static_cast<double>(body->impactSpeed));return box(0.0); }
    if (name == "physics_body_count" && argc == 0) { std::int64_t count=0;for(std::size_t i=1;i<ptsPhysics3D.bodies.size();++i)if(ptsPhysics3D.bodies[i].active)++count;return box(count); }
    return nullptr;
}

static bool ptsAudioInitialize() {
    if (ptsAudio3D.engine && ptsAudio3D.master) return true;
    if (ptsAudio3D.attempted) return false;
    ptsAudio3D.attempted = true;
    ptsAudio3D.module = LoadLibraryW(L"XAudio2_9.dll");
    if (!ptsAudio3D.module) ptsAudio3D.module = LoadLibraryW(L"XAudio2_8.dll");
    if (!ptsAudio3D.module) return false;
    using CreateFn = HRESULT (WINAPI*)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);
    auto create = reinterpret_cast<CreateFn>(GetProcAddress(ptsAudio3D.module, "XAudio2Create"));
    if (!create || FAILED(create(&ptsAudio3D.engine, 0, XAUDIO2_DEFAULT_PROCESSOR)) || !ptsAudio3D.engine) {
        ptsAudio3D.engine = nullptr;
        FreeLibrary(ptsAudio3D.module);
        ptsAudio3D.module = nullptr;
        return false;
    }
    if (FAILED(ptsAudio3D.engine->CreateMasteringVoice(&ptsAudio3D.master, 2, 44100, 0, nullptr, nullptr, AudioCategory_GameEffects))) {
        ptsAudio3D.engine->Release();
        ptsAudio3D.engine = nullptr;
        FreeLibrary(ptsAudio3D.module);
        ptsAudio3D.module = nullptr;
        return false;
    }
    ptsAudio3D.master->SetVolume(ptsAudio3D.masterVolume);
    return true;
}

static std::shared_ptr<PtsAudioClip> ptsAudioLoadWave(const std::string& filename) {
    const fs::path path = ptsResolveAssetPath(filename);
    std::error_code ec;
    const fs::path absolute = fs::weakly_canonical(path, ec);
    const std::wstring key = (ec ? path : absolute).wstring();
    const auto cached = ptsAudio3D.clips.find(key);
    if (cached != ptsAudio3D.clips.end()) return cached->second;
    std::ifstream input(windowsApiPath(path), std::ios::binary);
    if (!input) return {};
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 44 || size > 64 * 1024 * 1024) return {};
    input.seekg(0, std::ios::beg);
    std::vector<BYTE> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!input || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) return {};
    auto read16 = [&](std::size_t at) -> std::uint16_t { return static_cast<std::uint16_t>(bytes[at] | (static_cast<unsigned>(bytes[at + 1]) << 8)); };
    auto read32 = [&](std::size_t at) -> std::uint32_t { return static_cast<std::uint32_t>(bytes[at]) | (static_cast<std::uint32_t>(bytes[at + 1]) << 8) | (static_cast<std::uint32_t>(bytes[at + 2]) << 16) | (static_cast<std::uint32_t>(bytes[at + 3]) << 24); };
    std::size_t formatAt = 0, formatSize = 0, sampleAt = 0, sampleSize = 0;
    for (std::size_t at = 12; at + 8 <= bytes.size();) {
        const std::uint32_t chunkSize = read32(at + 4);
        const std::size_t content = at + 8;
        if (chunkSize > bytes.size() - content) return {};
        if (std::memcmp(bytes.data() + at, "fmt ", 4) == 0) { formatAt = content; formatSize = chunkSize; }
        else if (std::memcmp(bytes.data() + at, "data", 4) == 0) { sampleAt = content; sampleSize = chunkSize; }
        const std::size_t advance = static_cast<std::size_t>(chunkSize) + (chunkSize & 1u);
        if (advance > bytes.size() - content) break;
        at = content + advance;
    }
    if (formatAt == 0 || formatSize < 16 || sampleAt == 0 || sampleSize == 0) return {};
    auto clip = std::make_shared<PtsAudioClip>();
    clip->format.wFormatTag = read16(formatAt);
    clip->format.nChannels = read16(formatAt + 2);
    clip->format.nSamplesPerSec = read32(formatAt + 4);
    clip->format.nAvgBytesPerSec = read32(formatAt + 8);
    clip->format.nBlockAlign = read16(formatAt + 12);
    clip->format.wBitsPerSample = read16(formatAt + 14);
    clip->format.cbSize = 0;
    const bool pcm = clip->format.wFormatTag == WAVE_FORMAT_PCM && (clip->format.wBitsPerSample == 8 || clip->format.wBitsPerSample == 16);
    const bool floatingPoint = clip->format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT && clip->format.wBitsPerSample == 32;
    if ((!pcm && !floatingPoint) || (clip->format.nChannels != 1 && clip->format.nChannels != 2) ||
        clip->format.nSamplesPerSec < 8000 || clip->format.nSamplesPerSec > 192000 ||
        clip->format.nBlockAlign != clip->format.nChannels * (clip->format.wBitsPerSample / 8) ||
        sampleSize % clip->format.nBlockAlign != 0 || sampleSize > std::numeric_limits<UINT32>::max()) return {};
    clip->samples.assign(bytes.begin() + static_cast<std::ptrdiff_t>(sampleAt), bytes.begin() + static_cast<std::ptrdiff_t>(sampleAt + sampleSize));
    ptsAudio3D.clips.emplace(key, clip);
    return clip;
}

static std::shared_ptr<PtsAudioClip> ptsAudioLoadMp3(const std::string& filename) {
    const fs::path path = ptsResolveAssetPath(filename);
    std::error_code ec;
    const fs::path absolute = fs::weakly_canonical(path, ec);
    const std::wstring key = (ec ? path : absolute).wstring();
    const auto cached = ptsAudio3D.clips.find(key);
    if (cached != ptsAudio3D.clips.end()) return cached->second;
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com);
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    const bool mfStarted = SUCCEEDED(hr);
    IMFSourceReader* reader = nullptr;
    IMFMediaType* outputType = nullptr;
    auto clip = std::make_shared<PtsAudioClip>();
    if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&outputType);
    if (SUCCEEDED(hr)) hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (SUCCEEDED(hr)) hr = outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (SUCCEEDED(hr)) hr = outputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    if (SUCCEEDED(hr)) hr = outputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    if (SUCCEEDED(hr)) hr = outputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    if (SUCCEEDED(hr)) hr = outputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    if (SUCCEEDED(hr)) hr = outputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 176400);
    if (SUCCEEDED(hr)) hr = reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (SUCCEEDED(hr)) hr = reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    if (SUCCEEDED(hr)) hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, outputType);
    if (SUCCEEDED(hr)) {
        clip->format.wFormatTag = WAVE_FORMAT_PCM; clip->format.nChannels = 2; clip->format.nSamplesPerSec = 44100;
        clip->format.wBitsPerSample = 16; clip->format.nBlockAlign = 4; clip->format.nAvgBytesPerSec = 176400; clip->format.cbSize = 0;
        bool reachedEnd = false;
        while (!reachedEnd && SUCCEEDED(hr)) {
            DWORD flags = 0; IMFSample* sample = nullptr;
            hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
            if (SUCCEEDED(hr) && sample) {
                IMFMediaBuffer* buffer = nullptr;
                hr = sample->ConvertToContiguousBuffer(&buffer);
                if (SUCCEEDED(hr)) {
                    BYTE* data = nullptr; DWORD maximum = 0, current = 0;
                    hr = buffer->Lock(&data, &maximum, &current);
                    if (SUCCEEDED(hr)) {
                        if (current > 128U * 1024U * 1024U - std::min<std::size_t>(clip->samples.size(), 128U * 1024U * 1024U)) hr = E_OUTOFMEMORY;
                        else if (current != 0 && data) clip->samples.insert(clip->samples.end(), data, data + current);
                        else if (current != 0) hr = E_FAIL;
                        buffer->Unlock();
                    }
                    buffer->Release();
                }
                sample->Release();
            }
            reachedEnd = (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0;
        }
        if (SUCCEEDED(hr) && (clip->samples.empty() || clip->samples.size() % clip->format.nBlockAlign != 0)) hr = E_FAIL;
    }
    if (outputType) outputType->Release(); if (reader) reader->Release();
    if (mfStarted) MFShutdown();
    if (uninitialize) CoUninitialize();
    if (FAILED(hr)) return {};
    ptsAudio3D.clips[key] = clip;
    return clip;
}

static std::shared_ptr<PtsAudioClip> ptsAudioLoadOggVorbis(const std::string& filename) {
    const fs::path path=ptsResolveAssetPath(filename);
    std::error_code ec;
    const fs::path absolute=fs::weakly_canonical(path,ec);
    const std::wstring key=(ec?path:absolute).wstring();
    const auto cached=ptsAudio3D.clips.find(key);
    if(cached!=ptsAudio3D.clips.end())return cached->second;
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input)return {};
    const std::streamoff end=input.tellg();
    constexpr std::size_t maxCompressedBytes=128U*1024U*1024U;
    constexpr std::size_t maxPcmBytes=128U*1024U*1024U;
    if(end<=0||static_cast<std::uint64_t>(end)>maxCompressedBytes||end>std::numeric_limits<int>::max())return {};
    std::vector<unsigned char> encoded(static_cast<std::size_t>(end));
    input.seekg(0,std::ios::beg);
    if(!input.read(reinterpret_cast<char*>(encoded.data()),static_cast<std::streamsize>(encoded.size())))return {};
    int openError=0;
    stb_vorbis* decoder=stb_vorbis_open_memory(encoded.data(),static_cast<int>(encoded.size()),&openError,nullptr);
    if(!decoder)return {};
    const stb_vorbis_info info=stb_vorbis_get_info(decoder);
    if(info.channels<1||info.channels>8||info.sample_rate<8000||info.sample_rate>192000){stb_vorbis_close(decoder);return {};}
    const int outputChannels=info.channels==1?1:2;
    auto clip=std::make_shared<PtsAudioClip>();
    clip->format.wFormatTag=WAVE_FORMAT_PCM;
    clip->format.nChannels=static_cast<WORD>(outputChannels);
    clip->format.nSamplesPerSec=info.sample_rate;
    clip->format.wBitsPerSample=16;
    clip->format.nBlockAlign=static_cast<WORD>(outputChannels*2);
    clip->format.nAvgBytesPerSec=info.sample_rate*clip->format.nBlockAlign;
    clip->format.cbSize=0;
    std::vector<short> decoded(4096U*static_cast<std::size_t>(outputChannels));
    bool valid=true;
    for(;;){
        const int frames=stb_vorbis_get_samples_short_interleaved(decoder,outputChannels,decoded.data(),static_cast<int>(decoded.size()));
        if(frames<=0)break;
        const std::size_t bytes=static_cast<std::size_t>(frames)*static_cast<std::size_t>(outputChannels)*sizeof(short);
        if(bytes>maxPcmBytes-std::min(clip->samples.size(),maxPcmBytes)){valid=false;break;}
        const BYTE* begin=reinterpret_cast<const BYTE*>(decoded.data());
        clip->samples.insert(clip->samples.end(),begin,begin+bytes);
    }
    const int decodeError=stb_vorbis_get_error(decoder);
    stb_vorbis_close(decoder);
    if(!valid||decodeError!=VORBIS__no_error||clip->samples.empty()||clip->samples.size()%clip->format.nBlockAlign!=0)return {};
    ptsAudio3D.clips.emplace(key,clip);
    return clip;
}

static void ptsAudioPruneVoices() {
    for (auto it = ptsAudio3D.voices.begin(); it != ptsAudio3D.voices.end();) {
        XAUDIO2_VOICE_STATE state{};
        it->voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued == 0) {
            it->voice->DestroyVoice();
            it = ptsAudio3D.voices.erase(it);
        } else ++it;
    }
}

static bool ptsAudioPlay(const std::shared_ptr<PtsAudioClip>& clip, float volume, float pan) {
    if (!clip || !ptsAudioInitialize()) return false;
    ptsAudioPruneVoices();
    if (ptsAudio3D.voices.size() >= 48) return false;
    IXAudio2SourceVoice* voice = nullptr;
    if (FAILED(ptsAudio3D.engine->CreateSourceVoice(&voice, &clip->format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, nullptr, nullptr)) || !voice) return false;
    const float gain = std::clamp(volume, 0.0f, 1.0f);
    const float p = std::clamp(pan, -1.0f, 1.0f);
    const float angle = (p + 1.0f) * 0.7853981633974483f;
    const float left = std::cos(angle);
    const float right = std::sin(angle);
    voice->SetVolume(gain);
    float matrix[4]{};
    if (clip->format.nChannels == 1) {
        matrix[0] = left; matrix[1] = right;
    } else {
        matrix[0] = left * 1.41421356f; matrix[1] = 0.0f;
        matrix[2] = 0.0f; matrix[3] = right * 1.41421356f;
    }
    if (FAILED(voice->SetOutputMatrix(ptsAudio3D.master, clip->format.nChannels, 2, matrix, XAUDIO2_COMMIT_NOW))) { voice->DestroyVoice(); return false; }
    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = static_cast<UINT32>(clip->samples.size());
    buffer.pAudioData = clip->samples.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start())) { voice->DestroyVoice(); return false; }
    ptsAudio3D.voices.push_back({voice, clip});
    return true;
}

static void ptsAudioStopAll() {
    for (auto& active : ptsAudio3D.voices) if (active.voice) active.voice->DestroyVoice();
    ptsAudio3D.voices.clear();
}

static void ptsAudioShutdown() {
    ptsAudioStopAll();
    ptsAudio3D.clips.clear();
    if (ptsAudio3D.master) { ptsAudio3D.master->DestroyVoice(); ptsAudio3D.master = nullptr; }
    if (ptsAudio3D.engine) { ptsAudio3D.engine->StopEngine(); ptsAudio3D.engine->Release(); ptsAudio3D.engine = nullptr; }
    if (ptsAudio3D.module) { FreeLibrary(ptsAudio3D.module); ptsAudio3D.module = nullptr; }
    ptsAudio3D.attempted = false;
}

static std::shared_ptr<PtsAudioClip> ptsAudioMakeEffect(const std::string& kind, float strength) {
    std::string effect = kind;
    std::transform(effect.begin(), effect.end(), effect.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const bool impact = effect == "impact" || effect == "carpisma" || effect == "collision";
    const bool jump = effect == "jump" || effect == "zipla" || effect == "ziplama";
    const bool coin = effect == "coin" || effect == "pickup" || effect == "topla";
    const bool explosion = effect == "explosion" || effect == "patlama";
    const bool footstep = effect == "footstep" || effect == "step" || effect == "adim";
    const bool whoosh = effect == "whoosh" || effect == "wind" || effect == "ruzgar";
    if (!impact && !jump && !coin && !explosion && !footstep && !whoosh) return {};
    constexpr int sampleRate = 44100;
    const float duration = coin ? 0.20f : jump ? 0.24f : whoosh ? 0.42f : explosion ? 0.48f : 0.28f;
    const std::size_t count = static_cast<std::size_t>(sampleRate * duration);
    auto clip = std::make_shared<PtsAudioClip>();
    clip->format.wFormatTag = WAVE_FORMAT_PCM; clip->format.nChannels = 1; clip->format.nSamplesPerSec = sampleRate;
    clip->format.wBitsPerSample = 16; clip->format.nBlockAlign = 2; clip->format.nAvgBytesPerSec = sampleRate * 2; clip->format.cbSize = 0;
    clip->samples.resize(count * sizeof(std::int16_t));
    auto* pcm = reinterpret_cast<std::int16_t*>(clip->samples.data());
    const float intensity = std::clamp(strength, 0.15f, 2.5f);
    float phase = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / sampleRate;
        float frequency = 110.0f;
        float envelope = 0.0f;
        float noiseMix = 0.0f;
        if (impact) { frequency = 95.0f + 35.0f * intensity * std::exp(-t * 8.0f); envelope = std::exp(-t * 18.0f); noiseMix = std::exp(-t * 70.0f) * 0.55f; }
        else if (jump) { frequency = 190.0f + 430.0f * std::min(t / duration, 1.0f); envelope = std::sin(3.14159265f * t / duration) * std::exp(-t * 1.2f); }
        else if (coin) { frequency = t < 0.10f ? 740.0f : 990.0f; envelope = std::exp(-t * 8.0f); }
        else if (explosion) { frequency = 48.0f + 34.0f * std::exp(-t * 7.0f); envelope = std::exp(-t * 6.0f); noiseMix = std::exp(-t * 8.0f) * 0.75f; }
        else if (footstep) { frequency = 72.0f + 22.0f * std::exp(-t * 18.0f); envelope = std::exp(-t * 24.0f); noiseMix = std::exp(-t * 32.0f) * 0.3f; }
        else { frequency = 190.0f + 65.0f * std::sin(t * 7.0f); envelope = 0.55f + 0.45f * std::sin(t * 19.0f); noiseMix = 0.12f; }
        phase += 6.28318530718f * frequency / sampleRate;
        const float noise = static_cast<float>(static_cast<int>(randomEngineState()() & 0xffffu) - 32768) / 32768.0f;
        float sample = (std::sin(phase) * (1.0f - noiseMix) + noise * noiseMix) * envelope * std::min(intensity, 1.0f) * 0.82f;
        if (coin) sample += std::sin(phase * 2.01f) * envelope * 0.13f;
        pcm[i] = static_cast<std::int16_t>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
    }
    return clip;
}

static void* ptsAudio3DCall(const std::string& name, void** args, int argc) {
    auto number = [&](int index) { return static_cast<float>(numberOf(argument(args, index))); };
    if (name == "audio_set_listener" && argc == 6) {
        ptsAudio3D.listenerPosition = {number(0),number(1),number(2)};
        ptsAudio3D.listenerForward = ptsNormalize({number(3),number(4),number(5)});
        return nullptr;
    }
    if (name == "audio_set_master_volume" && argc == 1) {
        ptsAudio3D.masterVolume = std::clamp(number(0), 0.0f, 1.0f);
        if (ptsAudio3D.master) ptsAudio3D.master->SetVolume(ptsAudio3D.masterVolume);
        return nullptr;
    }
    if (name == "audio_play_sound" && (argc == 1 || argc == 2)) {
        auto clip = ptsAudioLoadWave(argumentString(args,0));
        const float volume = argc == 2 ? number(1) : 1.0f;
        const bool result = ptsAudioPlay(clip,volume,0.0f);
        if (!result) writeStderr("PRCYTERAS Engine3D audio: WAV acilamadi veya XAudio2 baslatilamadi. PCM 8/16-bit ya da IEEE float 32-bit WAV kullanin.\n");
        return box(result);
    }
    if (name == "audio_validate_mp3" && argc == 1) return box(static_cast<bool>(ptsAudioLoadMp3(argumentString(args,0))));
    if (name == "audio_play_mp3" && (argc == 1 || argc == 2)) {
        const float volume = argc == 2 ? number(1) : 1.0f;
        const bool result = ptsAudioPlay(ptsAudioLoadMp3(argumentString(args,0)), volume, 0.0f);
        if (!result) writeStderr("PRCYTERAS Engine3D audio: MP3 cozulemedi ya da XAudio2 ses cikisi yok.\n");
        return box(result);
    }
    if(name=="audio_validate_ogg"&&argc==1)return box(static_cast<bool>(ptsAudioLoadOggVorbis(argumentString(args,0))));
    if(name=="audio_play_ogg"&&(argc==1||argc==2)){
        const float volume=argc==2?number(1):1.0f;
        const bool result=ptsAudioPlay(ptsAudioLoadOggVorbis(argumentString(args,0)),volume,0.0f);
        if(!result)writeStderr("PRCYTERAS Engine3D audio: OGG Vorbis cozulemedi ya da XAudio2 ses cikisi yok.\n");
        return box(result);
    }
    if (name == "audio_play_sound_3d" && argc >= 4 && argc <= 6) {
        const std::string filename = argumentString(args,0);
        const PtsVec3 position{number(1),number(2),number(3)};
        const float volume = argc >= 5 ? number(4) : 1.0f;
        const float maxDistance = argc >= 6 ? std::max(0.1f,number(5)) : 40.0f;
        const PtsVec3 offset = ptsSub(position,ptsAudio3D.listenerPosition);
        const float distance = std::sqrt(std::max(0.0f,ptsDot(offset,offset)));
        if (distance >= maxDistance) return box(false);
        const PtsVec3 right = ptsNormalize(ptsCross(ptsAudio3D.listenerForward,{0,1,0}));
        const float pan = distance > 1.0e-4f ? std::clamp(ptsDot(ptsMul(offset,1.0f/distance),right),-1.0f,1.0f) : 0.0f;
        const float attenuation = 1.0f / (1.0f + 2.0f * distance / maxDistance + distance * distance / (maxDistance * maxDistance));
        const bool result = ptsAudioPlay(ptsAudioLoadWave(filename),volume*attenuation,pan);
        if (!result) writeStderr("PRCYTERAS Engine3D audio: 3B WAV sesi yuklenemedi veya oynatilamadi.\n");
        return box(result);
    }
    if (name == "audio_play_effect" && argc >= 1 && argc <= 3) {
        const std::string kind = argumentString(args,0);
        const float strength = argc >= 2 ? number(1) : 1.0f;
        const float volume = argc >= 3 ? number(2) : 0.8f;
        const bool result = ptsAudioPlay(ptsAudioMakeEffect(kind,strength),volume,0.0f);
        if (!result) writeStderr("PRCYTERAS Engine3D audio: efekt adi taninmadi veya ses sistemi baslatilamadi.\n");
        return box(result);
    }
    if (name == "audio_stop_all" && argc == 0) { ptsAudioStopAll(); return nullptr; }
    if (name == "audio_voice_count" && argc == 0) { ptsAudioPruneVoices(); return box(static_cast<std::int64_t>(ptsAudio3D.voices.size())); }
    return nullptr;
}
PtsMatrix4 ptsIdentity() {
    PtsMatrix4 m{}; m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f; return m;
}
PtsMatrix4 ptsMultiply(const PtsMatrix4& a, const PtsMatrix4& b) {
    PtsMatrix4 out{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            for (int k = 0; k < 4; ++k)
                out.m[row*4+col] += a.m[row*4+k] * b.m[k*4+col];
    return out;
}
PtsMatrix4 ptsScale(float x, float y, float z) {
    PtsMatrix4 m{}; m.m[0]=x; m.m[5]=y; m.m[10]=z; m.m[15]=1.0f; return m;
}
PtsMatrix4 ptsTranslate(float x, float y, float z) {
    PtsMatrix4 m=ptsIdentity(); m.m[12]=x; m.m[13]=y; m.m[14]=z; return m;
}
PtsMatrix4 ptsRotateX(float a) {
    PtsMatrix4 m=ptsIdentity(); const float c=std::cos(a), s=std::sin(a);
    m.m[5]=c; m.m[6]=s; m.m[9]=-s; m.m[10]=c; return m;
}
PtsMatrix4 ptsRotateY(float a) {
    PtsMatrix4 m=ptsIdentity(); const float c=std::cos(a), s=std::sin(a);
    m.m[0]=c; m.m[2]=-s; m.m[8]=s; m.m[10]=c; return m;
}
PtsMatrix4 ptsRotateZ(float a) {
    PtsMatrix4 m=ptsIdentity(); const float c=std::cos(a), s=std::sin(a);
    m.m[0]=c; m.m[1]=s; m.m[4]=-s; m.m[5]=c; return m;
}
PtsMatrix4 ptsLookAt(const float* eyeValues, const float* targetValues) {
    const PtsVec3 eye{eyeValues[0],eyeValues[1],eyeValues[2]};
    const PtsVec3 target{targetValues[0],targetValues[1],targetValues[2]};
    const PtsVec3 forward=ptsNormalize(ptsSub(target,eye));
    const PtsVec3 right=ptsNormalize(ptsCross({0,1,0},forward));
    const PtsVec3 up=ptsCross(forward,right);
    PtsMatrix4 m{};
    m.m[0]=right.x; m.m[1]=up.x; m.m[2]=forward.x;
    m.m[4]=right.y; m.m[5]=up.y; m.m[6]=forward.y;
    m.m[8]=right.z; m.m[9]=up.z; m.m[10]=forward.z;
    m.m[12]=-ptsDot(right,eye); m.m[13]=-ptsDot(up,eye); m.m[14]=-ptsDot(forward,eye); m.m[15]=1.0f;
    return m;
}
PtsMatrix4 ptsPerspective(float fovDegrees, float aspect, float nearPlane=0.1f, float farPlane=2000.0f) {
    const float y=1.0f/std::tan(std::clamp(fovDegrees,10.0f,150.0f)*0.008726646259971648f);
    const float x=y/std::max(aspect,0.01f);
    PtsMatrix4 m{}; m.m[0]=x; m.m[5]=y; m.m[10]=farPlane/(farPlane-nearPlane);
    m.m[11]=1.0f; m.m[14]=-nearPlane*farPlane/(farPlane-nearPlane); return m;
}

PtsMesh3D ptsCreateMesh(const std::vector<PtsVertex3D>& vertices) {
    PtsMesh3D mesh{};
    if (!ptsD3D11.device || vertices.empty() || vertices.size() > UINT_MAX / sizeof(PtsVertex3D)) return mesh;
    D3D11_BUFFER_DESC desc{}; desc.ByteWidth=static_cast<UINT>(vertices.size()*sizeof(PtsVertex3D));
    desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data{}; data.pSysMem=vertices.data();
    if (SUCCEEDED(ptsD3D11.device->CreateBuffer(&desc,&data,&mesh.vertices))) mesh.count=static_cast<UINT>(vertices.size());
    return mesh;
}

void ptsDestroyGraphicsTargets() {
    if(ptsD3D11.context)ptsD3D11.context->OMSetRenderTargets(0,nullptr,nullptr);
    ptsRelease(ptsD3D11.renderTarget); ptsRelease(ptsD3D11.depthView); ptsRelease(ptsD3D11.depthTexture);
}
bool ptsCreateGraphicsTargets(UINT width, UINT height) {
    if (!ptsD3D11.device || !ptsD3D11.swapChain || width == 0 || height == 0) return false;
    ptsDestroyGraphicsTargets();
    HRESULT hr=ptsD3D11.swapChain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0);
    if (FAILED(hr)) return false;
    ID3D11Texture2D* backBuffer=nullptr;
    hr=ptsD3D11.swapChain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&backBuffer));
    if (SUCCEEDED(hr)) hr=ptsD3D11.device->CreateRenderTargetView(backBuffer,nullptr,&ptsD3D11.renderTarget);
    ptsRelease(backBuffer);
    if (FAILED(hr)) return false;
    D3D11_TEXTURE2D_DESC depth{}; depth.Width=width; depth.Height=height; depth.MipLevels=1; depth.ArraySize=1;
    depth.Format=DXGI_FORMAT_D24_UNORM_S8_UINT; depth.SampleDesc.Count=1; depth.Usage=D3D11_USAGE_DEFAULT;
    depth.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    hr=ptsD3D11.device->CreateTexture2D(&depth,nullptr,&ptsD3D11.depthTexture);
    if (SUCCEEDED(hr)) hr=ptsD3D11.device->CreateDepthStencilView(ptsD3D11.depthTexture,nullptr,&ptsD3D11.depthView);
    if (FAILED(hr)) return false;
    D3D11_VIEWPORT viewport{}; viewport.Width=static_cast<float>(width); viewport.Height=static_cast<float>(height);
    viewport.MinDepth=0.0f; viewport.MaxDepth=1.0f;
    ptsD3D11.context->RSSetViewports(1,&viewport);
    ptsD3D11.context->OMSetRenderTargets(1,&ptsD3D11.renderTarget,ptsD3D11.depthView);
    return true;
}

const char* ptsVertexShaderSource = R"HLSL(
cbuffer Scene : register(b0) {
    row_major float4x4 mvp;
    row_major float4x4 world;
    float4 lightDirectionIntensity;
    float4 lightColor;
    float4 baseColor;
    float4 parameters;
};
struct VSInput { float3 position : POSITION; float2 uv : TEXCOORD0; float3 normal : NORMAL; };
struct VSOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; float3 normal : NORMAL; };
VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), mvp);
    output.uv = input.uv;
    output.normal = normalize(mul(float4(input.normal, 0.0), world).xyz);
    return output;
}
)HLSL";

const char* ptsPixelShaderSource = R"HLSL(
cbuffer Scene : register(b0) {
    row_major float4x4 mvp;
    row_major float4x4 world;
    float4 lightDirectionIntensity;
    float4 lightColor;
    float4 baseColor;
    float4 parameters;
};
Texture2D surfaceTexture : register(t0);
SamplerState surfaceSampler : register(s0);
struct PSInput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; float3 normal : NORMAL; };
float4 main(PSInput input) : SV_TARGET {
    float4 texel = parameters.x > 0.5 ? surfaceTexture.Sample(surfaceSampler, input.uv) : float4(1,1,1,1);
    float diffuse = saturate(dot(normalize(input.normal), normalize(-lightDirectionIntensity.xyz))) * lightDirectionIntensity.w;
    float3 illumination = 0.20 + diffuse * lightColor.rgb;
    return float4(baseColor.rgb * texel.rgb * illumination, baseColor.a * texel.a);
}
)HLSL";

const char* ptsSkyVertexShaderSource = R"HLSL(
struct VSOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
VSOutput main(uint vertexId : SV_VertexID) {
    float2 p[3] = { float2(-1.0, -1.0), float2(-1.0, 3.0), float2(3.0, -1.0) };
    VSOutput output;
    output.position = float4(p[vertexId], 0.0, 1.0);
    output.uv = float2(p[vertexId].x * 0.5 + 0.5, 0.5 - p[vertexId].y * 0.5);
    return output;
}
)HLSL";

const char* ptsSkyPixelShaderSource = R"HLSL(
cbuffer Sky : register(b0) {
    float4 cameraForward;
    float4 cameraRightAspect;
    float4 cameraUpTanHalfFov;
    float4 sunDirectionDaylight;
    float4 atmosphere;
};
struct PSInput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
float hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}
float noise2(float2 p) {
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(hash21(i), hash21(i + float2(1,0)), f.x),
                lerp(hash21(i + float2(0,1)), hash21(i + float2(1,1)), f.x), f.y);
}
float cloudNoise(float2 p) {
    float value = 0.0, weight = 0.55;
    [unroll] for (int i=0; i<4; ++i) { value += noise2(p) * weight; p = p * 2.03 + 13.7; weight *= 0.5; }
    return value / 1.03125;
}
float4 main(PSInput input) : SV_TARGET {
    float2 screen = float2(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0);
    float3 ray = normalize(cameraForward.xyz + cameraRightAspect.xyz * (screen.x * cameraRightAspect.w * cameraUpTanHalfFov.w)
                         + cameraUpTanHalfFov.xyz * (screen.y * cameraUpTanHalfFov.w));
    float day = sunDirectionDaylight.w;
    float twilight = saturate(1.0 - abs(day - 0.12) * 4.2);
    float horizon = saturate(1.0 - abs(ray.y) * 2.4);
    float vertical = pow(saturate(ray.y * 0.5 + 0.5), 0.68);
    float3 nightHorizon = float3(0.035, 0.055, 0.105);
    float3 nightZenith = float3(0.006, 0.012, 0.045);
    float3 dayHorizon = float3(0.66, 0.79, 0.91);
    float3 dayZenith = float3(0.105, 0.36, 0.72);
    float3 duskHorizon = float3(0.95, 0.34, 0.13);
    float3 duskZenith = float3(0.30, 0.20, 0.39);
    float3 low = lerp(nightHorizon, dayHorizon, day);
    float3 high = lerp(nightZenith, dayZenith, day);
    low = lerp(low, duskHorizon, twilight * 0.78);
    high = lerp(high, duskZenith, twilight * 0.58);
    float3 color = lerp(low, high, vertical);
    float3 sunDir = normalize(sunDirectionDaylight.xyz);
    float sunDot = dot(ray, sunDir);
    float sunDisk = smoothstep(0.99982, 0.99994, sunDot) * smoothstep(-0.08, 0.03, sunDir.y);
    float sunGlow = pow(saturate(sunDot), 48.0) * (0.20 + 0.8 * day);
    color += float3(1.0, 0.58, 0.22) * (sunDisk * (0.7 + day) + sunGlow * twilight * 0.42);
    float moonDot = dot(ray, -sunDir);
    float moon = smoothstep(0.99984, 0.99995, moonDot) * (1.0 - day);
    color += float3(0.62, 0.76, 1.0) * moon * 0.85;
    float2 cloudUv = ray.xz * (2.4 / max(ray.y + 0.22, 0.18));
    cloudUv += float2(atmosphere.z * 0.012, atmosphere.z * 0.004);
    float cloudAmount = saturate(atmosphere.x);
    float cloudField = cloudNoise(cloudUv);
    float cloud = smoothstep(0.58 - cloudAmount * 0.36, 0.84 - cloudAmount * 0.22, cloudField) * cloudAmount;
    cloud *= smoothstep(-0.16, 0.08, ray.y);
    float3 cloudColor = lerp(float3(0.18, 0.21, 0.30), float3(1.0, 0.97, 0.91), day);
    color = lerp(color, cloudColor, cloud * (0.58 + 0.34 * horizon));
    float2 starCell = floor(ray.xz * 640.0 + ray.y * 193.0);
    float star = step(0.9987, hash21(starCell)) * smoothstep(0.12, 0.52, ray.y) * (1.0 - day);
    float twinkle = 0.72 + 0.28 * sin(atmosphere.z * 2.4 + hash21(starCell + 17.0) * 6.28318);
    color += float3(0.72, 0.84, 1.0) * star * twinkle;
    return float4(saturate(color), 1.0);
}
)HLSL";

void ptsDestroyD3D11() {
    ptsAudioShutdown();
    if (ptsD3D11.mouseLocked) { ClipCursor(nullptr); ShowCursor(TRUE); ptsD3D11.mouseLocked=false; }
    for (std::size_t i=1;i<ptsD3D11.models.size();++i) ptsRelease(ptsD3D11.models[i].vertices);
    for (std::size_t i=1;i<ptsD3D11.textures.size();++i) ptsRelease(ptsD3D11.textures[i].view);
    ptsD3D11.models.clear(); ptsD3D11.models.resize(1);
    ptsD3D11.textures.clear(); ptsD3D11.textures.resize(1);
    ptsRelease(ptsD3D11.cube.vertices); ptsRelease(ptsD3D11.sphere.vertices);
    ptsRelease(ptsD3D11.depthState); ptsRelease(ptsD3D11.rasterizer); ptsRelease(ptsD3D11.blend);
    ptsRelease(ptsD3D11.sampler); ptsRelease(ptsD3D11.constants); ptsRelease(ptsD3D11.inputLayout);
    ptsRelease(ptsD3D11.vertexShader); ptsRelease(ptsD3D11.pixelShader);
    ptsRelease(ptsD3D11.skyVertexShader); ptsRelease(ptsD3D11.skyPixelShader);
    ptsRelease(ptsD3D11.skyConstants); ptsRelease(ptsD3D11.skyDepthState);
    ptsDestroyGraphicsTargets(); ptsRelease(ptsD3D11.swapChain);
    if (ptsD3D11.context) ptsD3D11.context->ClearState();
    ptsRelease(ptsD3D11.context); ptsRelease(ptsD3D11.device);
    if (ptsD3D11.window && IsWindow(ptsD3D11.window)) DestroyWindow(ptsD3D11.window);
    ptsD3D11.window=nullptr; ptsD3D11.running=false; ptsD3D11.frameOpen=false;
}

bool ptsCreatePrimitiveMeshes() {
    std::vector<PtsVertex3D> cube;
    auto face=[&](PtsVec3 a,PtsVec3 b,PtsVec3 c,PtsVec3 d,PtsVec3 n) {
        const PtsVertex3D v[]={{a.x,a.y,a.z,0,1,n.x,n.y,n.z},{b.x,b.y,b.z,1,1,n.x,n.y,n.z},{c.x,c.y,c.z,1,0,n.x,n.y,n.z},
                              {a.x,a.y,a.z,0,1,n.x,n.y,n.z},{c.x,c.y,c.z,1,0,n.x,n.y,n.z},{d.x,d.y,d.z,0,0,n.x,n.y,n.z}};
        cube.insert(cube.end(),std::begin(v),std::end(v));
    };
    const float h=0.5f;
    face({-h,-h,-h},{-h,h,-h},{h,h,-h},{h,-h,-h},{0,0,-1});
    face({h,-h,h},{h,h,h},{-h,h,h},{-h,-h,h},{0,0,1});
    face({-h,-h,h},{-h,h,h},{-h,h,-h},{-h,-h,-h},{-1,0,0});
    face({h,-h,-h},{h,h,-h},{h,h,h},{h,-h,h},{1,0,0});
    face({-h,h,-h},{-h,h,h},{h,h,h},{h,h,-h},{0,1,0});
    face({-h,-h,h},{-h,-h,-h},{h,-h,-h},{h,-h,h},{0,-1,0});
    ptsD3D11.cube=ptsCreateMesh(cube);
    std::vector<PtsVertex3D> sphere;
    constexpr int rings=16, segments=24;
    auto point=[](float latitude,float longitude) { const float c=std::cos(latitude); return PtsVec3{c*std::cos(longitude),std::sin(latitude),c*std::sin(longitude)}; };
    for(int r=0;r<rings;++r) for(int s=0;s<segments;++s) {
        const float lat0=-1.57079632679f+3.14159265359f*static_cast<float>(r)/rings;
        const float lat1=-1.57079632679f+3.14159265359f*static_cast<float>(r+1)/rings;
        const float lon0=6.28318530718f*static_cast<float>(s)/segments;
        const float lon1=6.28318530718f*static_cast<float>(s+1)/segments;
        PtsVec3 p[4]={point(lat0,lon0),point(lat0,lon1),point(lat1,lon1),point(lat1,lon0)};
        const float u0=static_cast<float>(s)/segments,u1=static_cast<float>(s+1)/segments;
        const float v0=static_cast<float>(r)/rings,v1=static_cast<float>(r+1)/rings;
        const PtsVertex3D verts[]={{p[0].x,p[0].y,p[0].z,u0,v0,p[0].x,p[0].y,p[0].z},{p[1].x,p[1].y,p[1].z,u1,v0,p[1].x,p[1].y,p[1].z},{p[2].x,p[2].y,p[2].z,u1,v1,p[2].x,p[2].y,p[2].z},
                                   {p[0].x,p[0].y,p[0].z,u0,v0,p[0].x,p[0].y,p[0].z},{p[2].x,p[2].y,p[2].z,u1,v1,p[2].x,p[2].y,p[2].z},{p[3].x,p[3].y,p[3].z,u0,v1,p[3].x,p[3].y,p[3].z}};
        sphere.insert(sphere.end(),std::begin(verts),std::end(verts));
    }
    ptsD3D11.sphere=ptsCreateMesh(sphere);
    return ptsD3D11.cube.vertices && ptsD3D11.sphere.vertices;
}

bool ptsInitD3D11(int width,int height,const std::string& title) {
    ptsDestroyD3D11();
    width=std::clamp(width,64,16384); height=std::clamp(height,64,16384);
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.style=CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wc.lpfnWndProc=[](HWND hwnd,UINT message,WPARAM wp,LPARAM lp)->LRESULT {
        if(message==WM_CLOSE){ptsD3D11.running=false;DestroyWindow(hwnd);return 0;}
        if(message==WM_DESTROY){ptsD3D11.running=false;ptsD3D11.window=nullptr;PostQuitMessage(0);return 0;}
        if(message==WM_KILLFOCUS && ptsD3D11.mouseLocked){ClipCursor(nullptr);ShowCursor(TRUE);ptsD3D11.mouseLocked=false;}
        if(message==WM_SIZE && ptsD3D11.swapChain && wp!=SIZE_MINIMIZED){
            if(!ptsCreateGraphicsTargets(LOWORD(lp),HIWORD(lp))) ptsD3D11.running=false;
            return 0;
        }
        return DefWindowProcW(hwnd,message,wp,lp);
    };
    wc.hInstance=GetModuleHandleW(nullptr); wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
    wc.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)); wc.lpszClassName=L"PRCYTERAS_D3D11_Window";
    if(!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;
    RECT rect{0,0,width,height}; AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    const std::wstring wideTitle=wide(title);
    ptsD3D11.window=CreateWindowExW(0,wc.lpszClassName,wideTitle.c_str(),WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
        rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(!ptsD3D11.window) return false;

    DXGI_SWAP_CHAIN_DESC swap{}; swap.BufferDesc.Width=width; swap.BufferDesc.Height=height;
    swap.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; swap.SampleDesc.Count=1;
    swap.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; swap.BufferCount=2; swap.OutputWindow=ptsD3D11.window;
    swap.Windowed=TRUE; swap.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL selected{};
    HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,3,D3D11_SDK_VERSION,
        &swap,&ptsD3D11.swapChain,&ptsD3D11.device,&selected,&ptsD3D11.context);
    if(FAILED(hr)) hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,3,D3D11_SDK_VERSION,
        &swap,&ptsD3D11.swapChain,&ptsD3D11.device,&selected,&ptsD3D11.context);
    if(FAILED(hr)){ptsDestroyD3D11();return false;}

    ID3DBlob *vs=nullptr,*ps=nullptr,*errors=nullptr;
    hr=D3DCompile(ptsVertexShaderSource,std::strlen(ptsVertexShaderSource),nullptr,nullptr,nullptr,"main","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&errors);
    if(FAILED(hr)){if(errors){writeStderr(static_cast<const char*>(errors->GetBufferPointer()));writeStderr("\n");}ptsRelease(errors);ptsDestroyD3D11();return false;}
    ptsRelease(errors);
    hr=D3DCompile(ptsPixelShaderSource,std::strlen(ptsPixelShaderSource),nullptr,nullptr,nullptr,"main","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&errors);
    if(FAILED(hr)){if(errors){writeStderr(static_cast<const char*>(errors->GetBufferPointer()));writeStderr("\n");}ptsRelease(errors);ptsRelease(vs);ptsDestroyD3D11();return false;}
    ptsRelease(errors);
    hr=ptsD3D11.device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&ptsD3D11.vertexShader);
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&ptsD3D11.pixelShader);
    D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,20,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateInputLayout(elements,3,vs->GetBufferPointer(),vs->GetBufferSize(),&ptsD3D11.inputLayout);
    ptsRelease(vs);ptsRelease(ps);
    ID3DBlob *skyVs=nullptr,*skyPs=nullptr;
    if(SUCCEEDED(hr))hr=D3DCompile(ptsSkyVertexShaderSource,std::strlen(ptsSkyVertexShaderSource),nullptr,nullptr,nullptr,"main","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&skyVs,&errors);
    if(FAILED(hr)&&errors){writeStderr(static_cast<const char*>(errors->GetBufferPointer()));writeStderr("\n");}
    ptsRelease(errors);
    if(SUCCEEDED(hr))hr=D3DCompile(ptsSkyPixelShaderSource,std::strlen(ptsSkyPixelShaderSource),nullptr,nullptr,nullptr,"main","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&skyPs,&errors);
    if(FAILED(hr)&&errors){writeStderr(static_cast<const char*>(errors->GetBufferPointer()));writeStderr("\n");}
    ptsRelease(errors);
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateVertexShader(skyVs->GetBufferPointer(),skyVs->GetBufferSize(),nullptr,&ptsD3D11.skyVertexShader);
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreatePixelShader(skyPs->GetBufferPointer(),skyPs->GetBufferSize(),nullptr,&ptsD3D11.skyPixelShader);
    ptsRelease(skyVs);ptsRelease(skyPs);
    D3D11_BUFFER_DESC constantDesc{}; constantDesc.ByteWidth=sizeof(PtsSceneConstants); constantDesc.Usage=D3D11_USAGE_DYNAMIC;
    constantDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER; constantDesc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateBuffer(&constantDesc,nullptr,&ptsD3D11.constants);
    D3D11_BUFFER_DESC skyConstantDesc{};skyConstantDesc.ByteWidth=sizeof(PtsSkyConstants);skyConstantDesc.Usage=D3D11_USAGE_DYNAMIC;
    skyConstantDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;skyConstantDesc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateBuffer(&skyConstantDesc,nullptr,&ptsD3D11.skyConstants);
    D3D11_SAMPLER_DESC samplerDesc{}; samplerDesc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU=samplerDesc.AddressV=samplerDesc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD=D3D11_FLOAT32_MAX;
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateSamplerState(&samplerDesc,&ptsD3D11.sampler);
    D3D11_BLEND_DESC blendDesc{}; blendDesc.RenderTarget[0].BlendEnable=TRUE;
    blendDesc.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA; blendDesc.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD; blendDesc.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA; blendDesc.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateBlendState(&blendDesc,&ptsD3D11.blend);
    D3D11_RASTERIZER_DESC rasterDesc{}; rasterDesc.FillMode=D3D11_FILL_SOLID; rasterDesc.CullMode=D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable=TRUE;
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateRasterizerState(&rasterDesc,&ptsD3D11.rasterizer);
    D3D11_DEPTH_STENCIL_DESC depthDesc{}; depthDesc.DepthEnable=TRUE; depthDesc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc=D3D11_COMPARISON_LESS; depthDesc.StencilEnable=FALSE;
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateDepthStencilState(&depthDesc,&ptsD3D11.depthState);
    D3D11_DEPTH_STENCIL_DESC skyDepthDesc{};skyDepthDesc.DepthEnable=FALSE;skyDepthDesc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    skyDepthDesc.DepthFunc=D3D11_COMPARISON_ALWAYS;skyDepthDesc.StencilEnable=FALSE;
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateDepthStencilState(&skyDepthDesc,&ptsD3D11.skyDepthState);
    if(SUCCEEDED(hr))hr=ptsCreateGraphicsTargets(width,height)?S_OK:E_FAIL;
    if(SUCCEEDED(hr) && !ptsCreatePrimitiveMeshes())hr=E_OUTOFMEMORY;
    if(FAILED(hr)){ptsDestroyD3D11();return false;}
    ptsD3D11.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ptsD3D11.context->IASetInputLayout(ptsD3D11.inputLayout);
    ptsD3D11.context->VSSetShader(ptsD3D11.vertexShader,nullptr,0);
    ptsD3D11.context->PSSetShader(ptsD3D11.pixelShader,nullptr,0);
    ptsD3D11.context->PSSetSamplers(0,1,&ptsD3D11.sampler);
    ptsD3D11.context->OMSetBlendState(ptsD3D11.blend,nullptr,0xffffffff);
    ptsD3D11.context->OMSetDepthStencilState(ptsD3D11.depthState,0);
    ptsD3D11.context->RSSetState(ptsD3D11.rasterizer);
    ShowWindow(ptsD3D11.window,SW_SHOW); UpdateWindow(ptsD3D11.window);
    ptsD3D11.running=true; ptsD3D11.startTime=ptsD3D11.lastFrame=std::chrono::steady_clock::now();
    return true;
}

std::vector<PtsVertex3D> ptsLoadObjVertices(const std::filesystem::path& path) {
    std::ifstream input(path);
    if(!input) return {};
    std::vector<PtsVec3> positions,normals;
    std::vector<std::array<float,2>> uvs;
    std::vector<PtsVertex3D> output;
    std::string line;
    auto indexOf=[](int index,std::size_t size)->std::size_t {
        const long long resolved=index>0?static_cast<long long>(index)-1:static_cast<long long>(size)+index;
        return resolved>=0 && static_cast<std::size_t>(resolved)<size?static_cast<std::size_t>(resolved):size;
    };
    while(std::getline(input,line)) {
        std::istringstream row(line); std::string tag; row>>tag;
        if(tag=="v") { PtsVec3 p{}; if(row>>p.x>>p.y>>p.z)positions.push_back(p); }
        else if(tag=="vn") { PtsVec3 n{}; if(row>>n.x>>n.y>>n.z)normals.push_back(ptsNormalize(n)); }
        else if(tag=="vt") { std::array<float,2> uv{}; if(row>>uv[0]>>uv[1])uvs.push_back(uv); }
        else if(tag=="f") {
            std::vector<std::array<int,3>> face;
            std::string part;
            while(row>>part) {
                if(part[0]=='#')break;
                std::array<int,3> indices{}; std::size_t start=0;
                for(int c=0;c<3;++c){const auto slash=part.find('/',start);const std::string sub=part.substr(start,slash==std::string::npos?std::string::npos:slash-start);if(!sub.empty())indices[c]=std::stoi(sub);if(slash==std::string::npos)break;start=slash+1;}
                face.push_back(indices);
            }
            for(std::size_t tri=1;tri+1<face.size();++tri) {
                const std::array<std::array<int,3>,3> indices={face[0],face[tri],face[tri+1]};
                const std::size_t pi[]={indexOf(indices[0][0],positions.size()),indexOf(indices[1][0],positions.size()),indexOf(indices[2][0],positions.size())};
                if(pi[0]>=positions.size()||pi[1]>=positions.size()||pi[2]>=positions.size())continue;
                const PtsVec3 normal=ptsNormalize(ptsCross(ptsSub(positions[pi[1]],positions[pi[0]]),ptsSub(positions[pi[2]],positions[pi[0]])));
                for(int k=0;k<3;++k) {
                    const auto& p=positions[pi[k]]; const std::size_t ui=indexOf(indices[k][1],uvs.size()),ni=indexOf(indices[k][2],normals.size());
                    const float u=ui<uvs.size()?uvs[ui][0]:0.0f, v=ui<uvs.size()?1.0f-uvs[ui][1]:0.0f;
                    const PtsVec3 n=ni<normals.size()?normals[ni]:normal;
                    output.push_back({p.x,p.y,p.z,u,v,n.x,n.y,n.z});
                }
            }
        }
    }
    return output;
}

int ptsLoadObj(const std::string& filename) {
    if(!ptsD3D11.device)return -1;
    const std::filesystem::path path=ptsResolveAssetPath(filename);
    try {
        auto vertices=ptsLoadObjVertices(path);
        if(vertices.empty()){writeStderr("PRCYTERAS Engine3D: OBJ modeli yuklenemedi: ");writeStderr(filename);writeStderr("\n");return -1;}
        PtsMesh3D mesh=ptsCreateMesh(vertices);
        if(!mesh.vertices)return -1;
        ptsD3D11.models.push_back(mesh); return static_cast<int>(ptsD3D11.models.size()-1);
    } catch(const std::exception& error) { writeStderr("PRCYTERAS Engine3D OBJ hatasi: ");writeStderr(error.what());writeStderr("\n");return -1; }
}

int ptsLoadTexture(const std::string& filename) {
    if(!ptsD3D11.device)return -1;
    const std::wstring path=ptsResolveAssetPath(filename).wstring();
    HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    const bool uninitialize=SUCCEEDED(com);
    IWICImagingFactory* factory=nullptr; IWICBitmapDecoder* decoder=nullptr; IWICBitmapFrameDecode* frame=nullptr;
    IWICFormatConverter* converter=nullptr; ID3D11Texture2D* texture=nullptr; ID3D11ShaderResourceView* view=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr))hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
    if(SUCCEEDED(hr))hr=converter->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0.0,WICBitmapPaletteTypeCustom);
    UINT width=0,height=0; if(SUCCEEDED(hr))hr=converter->GetSize(&width,&height);
    std::vector<BYTE> pixels;
    if(SUCCEEDED(hr) && width && height && static_cast<std::uint64_t>(width)*height<=67108864ULL) {
        pixels.resize(static_cast<std::size_t>(width)*height*4);
        hr=converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data());
    } else if(SUCCEEDED(hr)) hr=E_INVALIDARG;
    if(SUCCEEDED(hr)) {
        D3D11_TEXTURE2D_DESC desc{}; desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE; D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels.data();data.SysMemPitch=width*4;
        hr=ptsD3D11.device->CreateTexture2D(&desc,&data,&texture);
    }
    if(SUCCEEDED(hr))hr=ptsD3D11.device->CreateShaderResourceView(texture,nullptr,&view);
    ptsRelease(texture);ptsRelease(converter);ptsRelease(frame);ptsRelease(decoder);ptsRelease(factory);
    if(uninitialize)CoUninitialize();
    if(FAILED(hr)){writeStderr("PRCYTERAS Engine3D: doku yuklenemedi: ");writeStderr(filename);writeStderr("\n");ptsRelease(view);return -1;}
    ptsD3D11.textures.push_back({view}); return static_cast<int>(ptsD3D11.textures.size()-1);
}

void ptsUpdateMouse() {
    ptsD3D11.mouseDX=ptsD3D11.mouseDY=0;
    if(!ptsD3D11.mouseLocked||!ptsD3D11.window)return;
    RECT client{};GetClientRect(ptsD3D11.window,&client);POINT center{(client.right-client.left)/2,(client.bottom-client.top)/2};
    POINT screen=center;ClientToScreen(ptsD3D11.window,&screen);POINT current{};
    if(GetCursorPos(&current)){ptsD3D11.mouseDX=current.x-screen.x;ptsD3D11.mouseDY=current.y-screen.y;SetCursorPos(screen.x,screen.y);}
}

void ptsDrawMesh(const PtsMesh3D& mesh,float x,float y,float z,float rx,float ry,float rz,
                 float sx,float sy,float sz,const std::string& color,ID3D11ShaderResourceView* texture=nullptr) {
    if(!ptsD3D11.frameOpen||!mesh.vertices||!ptsD3D11.context)return;
    PtsMatrix4 world=ptsMultiply(ptsMultiply(ptsMultiply(ptsMultiply(ptsScale(sx,sy,sz),ptsRotateX(rx)),ptsRotateY(ry)),ptsRotateZ(rz)),ptsTranslate(x,y,z));
    RECT client{};GetClientRect(ptsD3D11.window,&client);const float aspect=static_cast<float>(std::max(1L,client.right-client.left))/static_cast<float>(std::max(1L,client.bottom-client.top));
    const PtsMatrix4 view=ptsLookAt(ptsD3D11.cameraPosition,ptsD3D11.cameraTarget);
    const PtsMatrix4 projection=ptsPerspective(ptsD3D11.fovDegrees,aspect);
    const PtsMatrix4 mvp=ptsMultiply(ptsMultiply(world,view),projection);
    PtsSceneConstants constants{};std::copy(std::begin(mvp.m),std::end(mvp.m),constants.mvp);
    std::copy(std::begin(world.m),std::end(world.m),constants.world);
    const PtsVec3 light=ptsNormalize({ptsD3D11.lightDirection[0],ptsD3D11.lightDirection[1],ptsD3D11.lightDirection[2]});
    constants.lightDirectionIntensity[0]=light.x;constants.lightDirectionIntensity[1]=light.y;constants.lightDirectionIntensity[2]=light.z;constants.lightDirectionIntensity[3]=ptsD3D11.lightIntensity;
    std::copy(std::begin(ptsD3D11.lightColor),std::end(ptsD3D11.lightColor),constants.lightColor);
    const COLORREF tint=colorOf(color);constants.baseColor[0]=GetRValue(tint)/255.0f;constants.baseColor[1]=GetGValue(tint)/255.0f;
    constants.baseColor[2]=GetBValue(tint)/255.0f;constants.baseColor[3]=1.0f;constants.parameters[0]=texture?1.0f:0.0f;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(ptsD3D11.context->Map(ptsD3D11.constants,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    std::memcpy(mapped.pData,&constants,sizeof(constants));ptsD3D11.context->Unmap(ptsD3D11.constants,0);
    const UINT stride=sizeof(PtsVertex3D),offset=0;ptsD3D11.context->IASetVertexBuffers(0,1,&mesh.vertices,&stride,&offset);
    ptsD3D11.context->VSSetConstantBuffers(0,1,&ptsD3D11.constants);ptsD3D11.context->PSSetConstantBuffers(0,1,&ptsD3D11.constants);
    ptsD3D11.context->PSSetShaderResources(0,1,&texture);ptsD3D11.context->Draw(mesh.count,0);
}

bool ptsDrawProceduralSky(float hour,float cloudCoverage) {
    if(!ptsD3D11.frameOpen||!ptsD3D11.context||!ptsD3D11.skyVertexShader||!ptsD3D11.skyPixelShader||!ptsD3D11.skyConstants)return false;
    RECT client{};GetClientRect(ptsD3D11.window,&client);
    const float width=static_cast<float>(std::max(1L,client.right-client.left));
    const float height=static_cast<float>(std::max(1L,client.bottom-client.top));
    PtsVec3 forward=ptsNormalize({ptsD3D11.cameraTarget[0]-ptsD3D11.cameraPosition[0],ptsD3D11.cameraTarget[1]-ptsD3D11.cameraPosition[1],ptsD3D11.cameraTarget[2]-ptsD3D11.cameraPosition[2]});
    PtsVec3 worldUp{0,1,0};
    if(std::abs(ptsDot(forward,worldUp))>0.999f)worldUp={0,0,1};
    const PtsVec3 right=ptsNormalize(ptsCross(worldUp,forward));
    const PtsVec3 up=ptsNormalize(ptsCross(forward,right));
    hour=std::fmod(hour,24.0f);if(hour<0.0f)hour+=24.0f;
    const float angle=(hour-6.0f)*0.2617993877991494f;
    const PtsVec3 sun{std::cos(angle),std::sin(angle),0.0f};
    float daylight=std::clamp((sun.y+0.12f)/0.36f,0.0f,1.0f);daylight=daylight*daylight*(3.0f-2.0f*daylight);
    PtsSkyConstants constants{};
    constants.forward[0]=forward.x;constants.forward[1]=forward.y;constants.forward[2]=forward.z;
    constants.rightAspect[0]=right.x;constants.rightAspect[1]=right.y;constants.rightAspect[2]=right.z;constants.rightAspect[3]=width/height;
    constants.upTanHalfFov[0]=up.x;constants.upTanHalfFov[1]=up.y;constants.upTanHalfFov[2]=up.z;
    constants.upTanHalfFov[3]=std::tan(std::clamp(ptsD3D11.fovDegrees,20.0f,140.0f)*0.008726646259971648f);
    constants.sunDirectionDaylight[0]=sun.x;constants.sunDirectionDaylight[1]=sun.y;constants.sunDirectionDaylight[2]=sun.z;constants.sunDirectionDaylight[3]=daylight;
    constants.atmosphere[0]=std::clamp(cloudCoverage,0.0f,1.0f);
    constants.atmosphere[2]=static_cast<float>(std::chrono::duration<double>(std::chrono::steady_clock::now()-ptsD3D11.startTime).count());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(ptsD3D11.context->Map(ptsD3D11.skyConstants,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return false;
    std::memcpy(mapped.pData,&constants,sizeof(constants));ptsD3D11.context->Unmap(ptsD3D11.skyConstants,0);
    ID3D11Buffer* emptyBuffer=nullptr;const UINT stride=0,offset=0;ptsD3D11.context->IASetInputLayout(nullptr);ptsD3D11.context->IASetVertexBuffers(0,1,&emptyBuffer,&stride,&offset);
    ptsD3D11.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ptsD3D11.context->VSSetShader(ptsD3D11.skyVertexShader,nullptr,0);ptsD3D11.context->PSSetShader(ptsD3D11.skyPixelShader,nullptr,0);
    ptsD3D11.context->VSSetConstantBuffers(0,1,&ptsD3D11.skyConstants);ptsD3D11.context->PSSetConstantBuffers(0,1,&ptsD3D11.skyConstants);
    ptsD3D11.context->OMSetDepthStencilState(ptsD3D11.skyDepthState,0);ptsD3D11.context->Draw(3,0);
    ptsD3D11.context->OMSetDepthStencilState(ptsD3D11.depthState,0);ptsD3D11.context->IASetInputLayout(ptsD3D11.inputLayout);
    ptsD3D11.context->VSSetShader(ptsD3D11.vertexShader,nullptr,0);ptsD3D11.context->PSSetShader(ptsD3D11.pixelShader,nullptr,0);
    return true;
}

int ptsVirtualKey(const std::string& key) {
    if(key.size()==1){const char c=static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));if(c>='A'&&c<='Z')return c;if(c>='0'&&c<='9')return c;}
    if(key=="SPACE")return VK_SPACE;if(key=="SHIFT")return VK_SHIFT;if(key=="CTRL"||key=="CONTROL")return VK_CONTROL;if(key=="BACKSPACE")return VK_BACK;
    if(key=="ENTER")return VK_RETURN;if(key=="ESC"||key=="ESCAPE")return VK_ESCAPE;
    if(key=="LEFT"||key=="SOL")return VK_LEFT;if(key=="RIGHT"||key=="SAG")return VK_RIGHT;
    if(key=="UP"||key=="YUKARI")return VK_UP;if(key=="DOWN"||key=="ASAGI")return VK_DOWN;
    if(key=="F1")return VK_F1;if(key=="F2")return VK_F2;if(key=="F3")return VK_F3;if(key=="F4")return VK_F4;
    if(key=="MOUSE_LEFT")return VK_LBUTTON;if(key=="MOUSE_RIGHT")return VK_RBUTTON;
    return 0;
}

void* engine3DCall(const std::string& name,void** args,int argc) {
    if(name.rfind("physics_",0)==0)return ptsPhysics3DCall(name,args,argc);
    if(name.rfind("audio_",0)==0)return ptsAudio3DCall(name,args,argc);
    if(name=="play_sound"||name=="play_sound_3d"||name=="play_effect"||name=="set_audio_listener"||name=="set_master_volume")return ptsAudio3DCall("audio_"+name,args,argc);
    if(name=="stop_sounds")return ptsAudio3DCall("audio_stop_all",args,argc);
    auto number=[&](int i){return static_cast<float>(numberOf(argument(args,i)));};
    if(name=="init_window"&&argc==3)return box(ptsInitD3D11(argumentInt(args,0),argumentInt(args,1),argumentString(args,2)));
    if(name=="is_running"){pumpMessages();if(!ptsD3D11.running)ptsAudioShutdown();return box(ptsD3D11.running);}
    if(name=="set_camera"&&argc==7){for(int i=0;i<3;++i){ptsD3D11.cameraPosition[i]=number(i);ptsD3D11.cameraTarget[i]=number(i+3);}ptsD3D11.fovDegrees=number(6);return nullptr;}
    if(name=="set_light"&&(argc==4||argc==7)){
        for(int i=0;i<3;++i)ptsD3D11.lightDirection[i]=number(i);
        if(argc==4){ptsD3D11.lightIntensity=std::clamp(number(3),0.0f,4.0f);ptsD3D11.lightColor[0]=ptsD3D11.lightColor[1]=ptsD3D11.lightColor[2]=1.0f;}
        else {ptsD3D11.lightIntensity=std::clamp(number(3),0.0f,4.0f);const COLORREF color=colorOf(argumentString(args,4));ptsD3D11.lightColor[0]=GetRValue(color)/255.0f;ptsD3D11.lightColor[1]=GetGValue(color)/255.0f;ptsD3D11.lightColor[2]=GetBValue(color)/255.0f;}
        return nullptr;
    }
    if(name=="set_vsync"&&argc==1){ptsD3D11.vsync=truthy(argument(args,0));return nullptr;}
    if(name=="request_close"){if(ptsD3D11.window)PostMessageW(ptsD3D11.window,WM_CLOSE,0,0);return nullptr;}
    if(name=="begin_frame"&&argc==1){
        pumpMessages();if(!ptsD3D11.running||!ptsD3D11.context||!ptsD3D11.renderTarget)return box(false);
        const auto now=std::chrono::steady_clock::now();ptsD3D11.deltaSeconds=std::clamp(std::chrono::duration<double>(now-ptsD3D11.lastFrame).count(),0.000001,0.25);ptsD3D11.lastFrame=now;ptsUpdateMouse();
        const COLORREF c=colorOf(argumentString(args,0));ptsD3D11.clear[0]=GetRValue(c)/255.0f;ptsD3D11.clear[1]=GetGValue(c)/255.0f;ptsD3D11.clear[2]=GetBValue(c)/255.0f;
        ptsD3D11.context->OMSetRenderTargets(1,&ptsD3D11.renderTarget,ptsD3D11.depthView);
        ptsD3D11.context->ClearRenderTargetView(ptsD3D11.renderTarget,ptsD3D11.clear);
        ptsD3D11.context->ClearDepthStencilView(ptsD3D11.depthView,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1.0f,0);
        ptsD3D11.frameOpen=true;return box(true);
    }
    if(name=="end_frame"){
        if(ptsD3D11.frameOpen&&ptsD3D11.swapChain){ptsD3D11.frameOpen=false;ptsD3D11.swapChain->Present(ptsD3D11.vsync?1:0,0);temporaryValuesState().clear();}
        return nullptr;
    }
    if(name=="delta_time")return box(ptsD3D11.deltaSeconds);
    if(name=="time_seconds")return box(std::chrono::duration<double>(std::chrono::steady_clock::now()-ptsD3D11.startTime).count());
    if(name=="draw_sky"&&argc==2)return box(ptsDrawProceduralSky(number(0),number(1)));
    if(name=="draw_sky_cycle"&&argc==3){const double secondsPerDay=numberOf(argument(args,1));if(secondsPerDay<=0.0)return box(false);const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-ptsD3D11.startTime).count();const float hour=static_cast<float>(numberOf(argument(args,0))+std::fmod(elapsed,secondsPerDay)*24.0/secondsPerDay);return box(ptsDrawProceduralSky(hour,number(2)));}
    if(name=="load_obj"&&argc==1)return box(static_cast<std::int64_t>(ptsLoadObj(argumentString(args,0))));
    if(name=="load_gltf"&&argc==1)return box(static_cast<std::int64_t>(ptsLoadGltf(argumentString(args,0))));
    if(name=="load_heightmap"&&argc==3)return box(static_cast<std::int64_t>(ptsLoadHeightmap(argumentString(args,0),numberOf(argument(args,1)),numberOf(argument(args,2)))));
    if(name=="load_texture"&&argc==1)return box(static_cast<std::int64_t>(ptsLoadTexture(argumentString(args,0))));
    if(name=="unload_model"&&argc==1){const int id=argumentInt(args,0);if(id>0&&static_cast<std::size_t>(id)<ptsD3D11.models.size()){ptsRelease(ptsD3D11.models[id].vertices);ptsD3D11.models[id].count=0;}return nullptr;}
    if(name=="unload_texture"&&argc==1){const int id=argumentInt(args,0);if(id>0&&static_cast<std::size_t>(id)<ptsD3D11.textures.size())ptsRelease(ptsD3D11.textures[id].view);return nullptr;}
    if(name=="draw_model"&&argc==9){const int id=argumentInt(args,0);if(id>0&&static_cast<std::size_t>(id)<ptsD3D11.models.size())ptsDrawMesh(ptsD3D11.models[id],number(1),number(2),number(3),number(4),number(5),number(6),number(7),number(7),number(7),argumentString(args,8));return nullptr;}
    if(name=="draw_textured_model"&&argc==9){const int id=argumentInt(args,0),tid=argumentInt(args,1);if(id>0&&static_cast<std::size_t>(id)<ptsD3D11.models.size()&&tid>0&&static_cast<std::size_t>(tid)<ptsD3D11.textures.size())ptsDrawMesh(ptsD3D11.models[id],number(2),number(3),number(4),number(5),number(6),number(7),number(8),number(8),number(8),"WHITE",ptsD3D11.textures[tid].view);return nullptr;}
    if(name=="draw_cube"&&argc==8){ptsDrawMesh(ptsD3D11.cube,number(0),number(1),number(2),number(4),number(5),number(6),number(3),number(3),number(3),argumentString(args,7));return nullptr;}
    if(name=="draw_box"&&argc==7){ptsDrawMesh(ptsD3D11.cube,number(0),number(1),number(2),0,0,0,number(3),number(4),number(5),argumentString(args,6));return nullptr;}
    if(name=="draw_sphere"&&argc==5){const float r=std::max(0.0f,number(3));ptsDrawMesh(ptsD3D11.sphere,number(0),number(1),number(2),0,0,0,r,r,r,argumentString(args,4));return nullptr;}
    if(name=="key_down"&&argc==1){const int key=ptsVirtualKey(argumentString(args,0));return box(key && (GetAsyncKeyState(key)&0x8000));}
    if(name=="set_mouse_locked"&&argc==1){const bool lock=truthy(argument(args,0));if(lock&&!ptsD3D11.mouseLocked&&ptsD3D11.window){RECT r{};GetClientRect(ptsD3D11.window,&r);POINT a{r.left,r.top},b{r.right,r.bottom};ClientToScreen(ptsD3D11.window,&a);ClientToScreen(ptsD3D11.window,&b);r={a.x,a.y,b.x,b.y};ClipCursor(&r);ShowCursor(FALSE);ptsD3D11.mouseLocked=true;}else if(!lock&&ptsD3D11.mouseLocked){ClipCursor(nullptr);ShowCursor(TRUE);ptsD3D11.mouseLocked=false;}return nullptr;}
    if(name=="mouse_delta_x")return box(static_cast<std::int64_t>(ptsD3D11.mouseDX));
    if(name=="mouse_delta_y")return box(static_cast<std::int64_t>(ptsD3D11.mouseDY));
    return nullptr;
}
