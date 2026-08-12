#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h/consumer.h>
#include <linux/unaligned.h>


/* media and v4l2 includes */
#include <media/media-entity.h>
#include <media/v4l2-cci.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>


/* Register Map for IMX296 */
#define IMX296_REG_8BIT(n)				((1 << 16) | (n))
#define IMX296_REG_16BIT(n)				((2 << 16) | (n))
#define IMX296_REG_24BIT(n)				((3 << 16) | (n))
#define IMX296_REG_SIZE_SHIFT				16
#define IMX296_REG_ADDR_MASK				0xffff

#define IMX296_CTRL00					IMX296_REG_8BIT(0x3000)
#define IMX296_CTRL00_STANDBY				BIT(0)
#define IMX296_CTRL08					IMX296_REG_8BIT(0x3008)
#define IMX296_CTRL08_REGHOLD				BIT(0)
#define IMX296_CTRL0A					IMX296_REG_8BIT(0x300a)
#define IMX296_CTRL0A_XMSTA				BIT(0)
#define IMX296_CTRL0B					IMX296_REG_8BIT(0x300b)
#define IMX296_CTRL0B_TRIGEN				BIT(0)
#define IMX296_CTRL0D					IMX296_REG_8BIT(0x300d)
#define IMX296_CTRL0D_WINMODE_ALL			(0 << 0)
#define IMX296_CTRL0D_WINMODE_FD_BINNING		(2 << 0)
#define IMX296_CTRL0D_HADD_ON_BINNING			BIT(5)
#define IMX296_CTRL0D_SAT_CNT				BIT(6)
#define IMX296_CTRL0E					IMX296_REG_8BIT(0x300e)
#define IMX296_CTRL0E_VREVERSE				BIT(0)
#define IMX296_CTRL0E_HREVERSE				BIT(1)
#define IMX296_VMAX					    IMX296_REG_24BIT(0x3010)
#define IMX296_HMAX					    IMX296_REG_16BIT(0x3014)
#define IMX296_TMDCTRL					IMX296_REG_8BIT(0x301d)
#define IMX296_TMDCTRL_LATCH				BIT(0)
#define IMX296_TMDOUT					IMX296_REG_16BIT(0x301e)
#define IMX296_TMDOUT_MASK				0x3ff
#define IMX296_WDSEL					IMX296_REG_8BIT(0x3021)
#define IMX296_WDSEL_NORMAL				(0 << 0)
#define IMX296_WDSEL_MULTI_2				(1 << 0)
#define IMX296_WDSEL_MULTI_4				(3 << 0)
#define IMX296_BLKLEVELAUTO				IMX296_REG_8BIT(0x3022)
#define IMX296_BLKLEVELAUTO_ON				0x01
#define IMX296_BLKLEVELAUTO_OFF				0xf0
#define IMX296_SST					    IMX296_REG_8BIT(0x3024)
#define IMX296_SST_EN					BIT(0)
#define IMX296_CTRLTOUT					IMX296_REG_8BIT(0x3026)
#define IMX296_CTRLTOUT_TOUT1SEL_LOW			(0 << 0)
#define IMX296_CTRLTOUT_TOUT1SEL_PULSE			(3 << 0)
#define IMX296_CTRLTOUT_TOUT2SEL_LOW			(0 << 2)
#define IMX296_CTRLTOUT_TOUT2SEL_PULSE			(3 << 2)
#define IMX296_CTRLTRIG					IMX296_REG_8BIT(0x3029)
#define IMX296_CTRLTRIG_TOUT1_SEL_LOW			(0 << 0)
#define IMX296_CTRLTRIG_TOUT1_SEL_PULSE1		(1 << 0)
#define IMX296_CTRLTRIG_TOUT2_SEL_LOW			(0 << 4)
#define IMX296_CTRLTRIG_TOUT2_SEL_PULSE2		(2 << 4)
#define IMX296_SYNCSEL					IMX296_REG_8BIT(0x3036)
#define IMX296_SYNCSEL_NORMAL				0xc0
#define IMX296_SYNCSEL_HIZ				0xf0
#define IMX296_PULSE1					IMX296_REG_8BIT(0x306d)
#define IMX296_PULSE1_EN_NOR				BIT(0)
#define IMX296_PULSE1_EN_TRIG				BIT(1)
#define IMX296_PULSE1_POL_HIGH				(0 << 2)
#define IMX296_PULSE1_POL_LOW				(1 << 2)
#define IMX296_PULSE1_UP				IMX296_REG_24BIT(0x3070)
#define IMX296_PULSE1_DN				IMX296_REG_24BIT(0x3074)
#define IMX296_PULSE2					IMX296_REG_8BIT(0x3079)
#define IMX296_PULSE2_EN_NOR				BIT(0)
#define IMX296_PULSE2_EN_TRIG				BIT(1)
#define IMX296_PULSE2_POL_HIGH				(0 << 2)
#define IMX296_PULSE2_POL_LOW				(1 << 2)
#define IMX296_PULSE2_UP				IMX296_REG_24BIT(0x307c)
#define IMX296_PULSE2_DN				IMX296_REG_24BIT(0x3080)
#define IMX296_INCKSEL(n)				IMX296_REG_8BIT(0x3089 + (n))
#define IMX296_SHS1					    IMX296_REG_24BIT(0x308d)
#define IMX296_SHS2					    IMX296_REG_24BIT(0x3090)
#define IMX296_SHS3					    IMX296_REG_24BIT(0x3094)
#define IMX296_SHS4					    IMX296_REG_24BIT(0x3098)
#define IMX296_VBLANKLP					IMX296_REG_8BIT(0x309c)
#define IMX296_VBLANKLP_NORMAL				0x04
#define IMX296_VBLANKLP_LOW_POWER			0x2c
#define IMX296_EXP_CNT					IMX296_REG_8BIT(0x30a3)
#define IMX296_EXP_CNT_RESET				BIT(0)
#define IMX296_EXP_MAX					IMX296_REG_16BIT(0x30a6)
#define IMX296_VINT					    IMX296_REG_8BIT(0x30aa)
#define IMX296_VINT_EN					BIT(0)
#define IMX296_LOWLAGTRG				IMX296_REG_8BIT(0x30ae)
#define IMX296_LOWLAGTRG_FAST				BIT(0)
#define IMX296_I2CCTRL					IMX296_REG_8BIT(0x30ef)
#define IMX296_I2CCTRL_I2CACKEN				BIT(0)

#define IMX296_SENSOR_INFO				IMX296_REG_16BIT(0x3148)
#define IMX296_SENSOR_INFO_MONO				BIT(15)
#define IMX296_SENSOR_INFO_IMX296LQ			0x4a00
#define IMX296_SENSOR_INFO_IMX296LL			0xca00
#define IMX296_S_SHSA					IMX296_REG_16BIT(0x31ca)
#define IMX296_S_SHSB					IMX296_REG_16BIT(0x31d2)
/*
 * Registers 0x31c8 to 0x31cd, 0x31d0 to 0x31d5, 0x31e2, 0x31e3, 0x31ea and
 * 0x31eb are related to exposure mode but otherwise not documented.
 */

#define IMX296_GAINCTRL					IMX296_REG_8BIT(0x3200)
#define IMX296_GAINCTRL_WD_GAIN_MODE_NORMAL		0x01
#define IMX296_GAINCTRL_WD_GAIN_MODE_MULTI		0x41
#define IMX296_GAIN					    IMX296_REG_16BIT(0x3204)
#define IMX296_GAIN_MIN					0
#define IMX296_GAIN_MAX					480
#define IMX296_GAIN1					IMX296_REG_16BIT(0x3208)
#define IMX296_GAIN2					IMX296_REG_16BIT(0x320c)
#define IMX296_GAIN3					IMX296_REG_16BIT(0x3210)
#define IMX296_GAINDLY					IMX296_REG_8BIT(0x3212)
#define IMX296_GAINDLY_NONE				0x08
#define IMX296_GAINDLY_1FRAME				0x09
#define IMX296_PGCTRL					IMX296_REG_8BIT(0x3238)
#define IMX296_PGCTRL_REGEN				BIT(0)
#define IMX296_PGCTRL_THRU				BIT(1)
#define IMX296_PGCTRL_CLKEN				BIT(2)
#define IMX296_PGCTRL_MODE(n)				((n) << 3)
#define IMX296_PGHPOS					IMX296_REG_16BIT(0x3239)
#define IMX296_PGVPOS					IMX296_REG_16BIT(0x323c)
#define IMX296_PGHPSTEP					IMX296_REG_8BIT(0x323e)
#define IMX296_PGVPSTEP					IMX296_REG_8BIT(0x323f)
#define IMX296_PGHPNUM					IMX296_REG_8BIT(0x3240)
#define IMX296_PGVPNUM					IMX296_REG_8BIT(0x3241)
#define IMX296_PGDATA1					IMX296_REG_16BIT(0x3244)
#define IMX296_PGDATA2					IMX296_REG_16BIT(0x3246)
#define IMX296_PGHGSTEP					IMX296_REG_8BIT(0x3249)
#define IMX296_BLKLEVEL					IMX296_REG_16BIT(0x3254)

#define IMX296_FID0_ROI					IMX296_REG_8BIT(0x3300)
#define IMX296_FID0_ROIH1ON				BIT(0)
#define IMX296_FID0_ROIV1ON				BIT(1)
#define IMX296_FID0_ROIPH1				IMX296_REG_16BIT(0x3310)
#define IMX296_FID0_ROIPV1				IMX296_REG_16BIT(0x3312)
#define IMX296_FID0_ROIWH1				IMX296_REG_16BIT(0x3314)
#define IMX296_FID0_ROIWH1_MIN				80
#define IMX296_FID0_ROIWV1				IMX296_REG_16BIT(0x3316)
#define IMX296_FID0_ROIWV1_MIN				4

#define IMX296_CM_HSST_STARTTMG				IMX296_REG_16BIT(0x4018)
#define IMX296_CM_HSST_ENDTMG				IMX296_REG_16BIT(0x401a)
#define IMX296_DA_HSST_STARTTMG				IMX296_REG_16BIT(0x404d)
#define IMX296_DA_HSST_ENDTMG				IMX296_REG_16BIT(0x4050)
#define IMX296_LM_HSST_STARTTMG				IMX296_REG_16BIT(0x4094)
#define IMX296_LM_HSST_ENDTMG				IMX296_REG_16BIT(0x4096)
#define IMX296_SST_SIEASTA1_SET				IMX296_REG_8BIT(0x40c9)
#define IMX296_SST_SIEASTA1PRE_1U			IMX296_REG_16BIT(0x40cc)
#define IMX296_SST_SIEASTA1PRE_1D			IMX296_REG_16BIT(0x40ce)
#define IMX296_SST_SIEASTA1PRE_2U			IMX296_REG_16BIT(0x40d0)
#define IMX296_SST_SIEASTA1PRE_2D			IMX296_REG_16BIT(0x40d2)
#define IMX296_HSST					IMX296_REG_8BIT(0x40dc)
#define IMX296_HSST_EN					BIT(2)

#define IMX296_CKREQSEL					IMX296_REG_8BIT(0x4101)
#define IMX296_CKREQSEL_HS				BIT(2)
#define IMX296_GTTABLENUM				IMX296_REG_8BIT(0x4114)
#define IMX296_CTRL418C					IMX296_REG_8BIT(0x418c)


/* USEFUL MACROS */
#define IMX296_NUM_SUPPLIES             3U
#define IMX296_MAX_WIDTH                1456U
#define IMX296_MAX_HEIGHT               1088U
#define IMX296_BIT_RATE                 1188000000ULL
#define IMX296_PIXEL_RATE               IMX296_BIT_RATE / 10ULL;
#define IMX296_INCK_FREQ                74250000UL
#define IMX296_HMAX_FIXED_VAL           1110U;


/* follows specific sequence specified in imx296lqr datasheet */
static const * char const imx296_supply_names[IMX296_NUM_SUPPLIES] =
{
    "dvdd",
    "ovdd",
    "avdd",
};

/* clk params needed for PLL setup */
struct imx296_clk_params {
    unsigned int freq, /* frequencies specified in datasheet */
    u8 incksel[4],     /* magic numbers provided in datahset pg 39 */
    u8 ctrl418c,       /* magic numbers provided in datahset pg 43  */
};

static const struct imx296_clk_params clk_params[] = {
    { 37125000, { 0x80, 0x0b, 0x80, 0x08 }, 116 },
    { 54000000, { 0xb0, 0x0f, 0xb0, 0x0c }, 168 },
    { 74250000, { 0x80, 0x0f, 0x80, 0x0c }, 232 },
};


/* V4L2 subdev Function Prototypes for IMX296 
    - probe() and remove()
    - s_stream()
    - s_ctrl()
    - log_status()
    - enum_mbus_code()
    - set_fmt()
    - get_fmt()
    - 
*/
static int imx296_set_stream();           //v4l2_subdev_video_ops
static int imx296_log_status();         //v4l2_subdev_core_ops

/* V4L2 Controls */
static int imx296_ctrl_init(struct imx296* sensor);
static int imx296_set_ctrl(struct v4l2_ctrl *ctrl);             //v4l2_subdev_ctrl_ops

/* v4l2 subdev pad ops */
static int imx296_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_state* state, struct v4l2_subdev_mbus_code_enum *code);     //v4l2_subdev_pad_ops
static int imx296_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state* state, struct v4l2_subdev_format *format);
static int imx296_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state* state, struct v4l2_subdev_format *format);
static int imx296_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_state* state, struct v4l2_subdev_frame_size_enum *fse);
static int imx296_get_selection(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_selection *sel);
static int imx296_set_selection(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_selection *sel);

/* Power related functions */
static int imx296_power_on(struct imx296* sensor);
static void imx296_power_off(struct imx296* sensor);
static int imx296_runtime_suspend();
static int imx296_runtime_resume();

/* init and remove functions */
static int imx296_probe(struct i2c_client *client);
static void imx296_remove(struct i2c_client *client);
static int imx296_subdev_init(struct imx296* sensor);


/* v4l2 subdev internal ops */
static int imx296_init_state(struct v4l2_subdev* sd, struct v4l2_subdev_state state);


/* helper functions */
static int imx296_stream_on(struct imx296* sensor);
static int imx296_stream_off(struct imx296* sensor);
static int imx296_setup(struct imx296* sensor, v4l2_subdev_state* state);

/* struct definitions 
    - need a main imx296 struct which should contain the following
        - v4l2_subdev struct
        - v4l2_subdev_video_ops  (.s_stream function is attached to this struct)
        - v4l2_subdev_pad_ops    (.enum_mbus_code, .enum_frame_size, .get_fmt, .set_fmt, .get_selection)
        - v4l2_subdev_core_ops   (.log_status)
        - v4l2_subdev_ops        (address of subdev_video_ops, subdev_pad_ops, subdev_core_ops)
        - 

*/

struct v4l2_subdev_pad_ops imx296_pad_ops =  {
    .enum_mbus_code =  //need to pass function pointers of imx-functions to struct
    .enum_frame_size = 
    .get_fmt =
    .set_fmt = 
    .get_selection =

};

struct v4l2_subdev_video_ops imx296_video_ops = {
    .s_stream = imx296_set_stream,

};

struct v4l2_subdev_ops imx296_subdev_ops = {
    .video = &imx296_video_ops,
    .pad = &imx296_pad_ops,

};


struct v4l2_ctrl_ops imx296_ctrl_ops = {
    .s_ctrl = imx296_set_ctrl,

};

//thie struct is for funtime pm
struct dev_pm_ops imx296_pm_ops = {
    //RUNTIME_PM_OPS(imx296_runtime_suspend, imx296_runtime_resume, NULL);
};

struct v4l2_subdev_internal_ops imx296_internal_ops = {
    .init_state = imx296_init_state,
};

struct imx296 {

    struct v4l2_subdev sd;
    struct media_pad pad;
    struct i2c_client *client;
    bool mono;


    /* V4L2 Controls */
    struct v4l2_ctrl_handler ctrl_handler;
    struct v4l2_ctrl   *exposure;
    struct v4l2_ctrl   *gain;
    struct v4l2_ctrl   *hblank;
    struct v4l2_ctrl   *vblank;  
    struct v4l2_ctrl   *hflip;
    struct v4l2_ctrl   *vflip;

    /* I2C Register Map */
    struct regmap   *regmap;

    /* Power / Clocks / Regulators */
    struct clk      *xclk;
    struct gpio_desc *rst_gpio;
    struct regulator_bulk_data supplies[IMX296_NUM_SUPPLIES];

};

/* init table for imx296_setup */
/*
 * This table is extracted from vendor data that is entirely undocumented. The
 * first register write is required to activate the CSI-2 output. The other
 * entries may or may not be optional?
 */
struct cci_reg_sequence imx296_init_table[] = {
	{ IMX296_REG_8BIT(0x3005), 0xf0 },
	{ IMX296_REG_8BIT(0x309e), 0x04 },
	{ IMX296_REG_8BIT(0x30a0), 0x04 },
	{ IMX296_REG_8BIT(0x30a1), 0x3c },
	{ IMX296_REG_8BIT(0x30a4), 0x5f },
	{ IMX296_REG_8BIT(0x30a8), 0x91 },
	{ IMX296_REG_8BIT(0x30ac), 0x28 },
	{ IMX296_REG_8BIT(0x30af), 0x09 },
	{ IMX296_REG_8BIT(0x30df), 0x00 },
	{ IMX296_REG_8BIT(0x3165), 0x00 },
	{ IMX296_REG_8BIT(0x3169), 0x10 },
	{ IMX296_REG_8BIT(0x316a), 0x02 },
	{ IMX296_REG_8BIT(0x31c8), 0xf3 },	/* Exposure-related */
	{ IMX296_REG_8BIT(0x31d0), 0xf4 },	/* Exposure-related */
	{ IMX296_REG_8BIT(0x321a), 0x00 },
	{ IMX296_REG_8BIT(0x3226), 0x02 },
	{ IMX296_REG_8BIT(0x3256), 0x01 },
	{ IMX296_REG_8BIT(0x3541), 0x72 },
	{ IMX296_REG_8BIT(0x3516), 0x77 },
	{ IMX296_REG_8BIT(0x350b), 0x7f },
	{ IMX296_REG_8BIT(0x3758), 0xa3 },
	{ IMX296_REG_8BIT(0x3759), 0x00 },
	{ IMX296_REG_8BIT(0x375a), 0x85 },
	{ IMX296_REG_8BIT(0x375b), 0x00 },
	{ IMX296_REG_8BIT(0x3832), 0xf5 },
	{ IMX296_REG_8BIT(0x3833), 0x00 },
	{ IMX296_REG_8BIT(0x38a2), 0xf6 },
	{ IMX296_REG_8BIT(0x38a3), 0x00 },
	{ IMX296_REG_8BIT(0x3a00), 0x80 },
	{ IMX296_REG_8BIT(0x3d48), 0xa3 },
	{ IMX296_REG_8BIT(0x3d49), 0x00 },
	{ IMX296_REG_8BIT(0x3d4a), 0x85 },
	{ IMX296_REG_8BIT(0x3d4b), 0x00 },
	{ IMX296_REG_8BIT(0x400e), 0x58 },
	{ IMX296_REG_8BIT(0x4014), 0x1c },
	{ IMX296_REG_8BIT(0x4041), 0x2a },
	{ IMX296_REG_8BIT(0x40a2), 0x06 },
	{ IMX296_REG_8BIT(0x40c1), 0xf6 },
	{ IMX296_REG_8BIT(0x40c7), 0x0f },
	{ IMX296_REG_8BIT(0x40c8), 0x00 },
	{ IMX296_REG_8BIT(0x4174), 0x00 },
};