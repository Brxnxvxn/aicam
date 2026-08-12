#include "imx296.h"

/* constants */
#define MAX_20_BITS                 0xFFFFFU
#define EXPOSURE_DEF_VALUE          1104U


/* inline function returns the address of imx296 struct given v4l2_subdev sd member address 
    -uses the container_of linux kernel macro
*/
static inline struct imx296* to_imx296(struct v4l2_subdev *_sd)
{
    return container_of(_sd, struct imx296, sd);
}



/* V4L2 Controls */

static const char * const imx296_test_pattern_menu[] = {
	"Disabled",
	"Multiple Pixels",
	"Sequence 1",
	"Sequence 2",
	"Gradient",
	"Row",
	"Column",
	"Cross",
	"Stripe",
	"Checks",
};


static int imx296_set_ctrl(struct v4l2_ctrl *ctrl)
{

    unsigned int vmax = 0;
    int ret = 0;

    /* get pointer to imx296 struct */
    struct imx296* sensor = container_of(ctrl->handler, struct imx296, ctrl_handler);

    /* need to get current format before setting certain controls */
    struct v4l2_subdev_state* state = v4l2_subdev_get_locked_active_state(sensor->sd);
    struct v4l2_mbus_framefmt* fmt = v4l2_subdev_state_get_format(state, 0);



    /* perform register writes based on ctrl id */
    switch(ctrl->id) 
    {
        case V4L2_CID_EXPOSURE:
            vmax = fmt->height + sensor->vblank->cur.val;
            
            /* val cannot exceed vmax */
            ctrl->val = min_t(int, ctrl->val, vmax);

            cci_write(sensor->regmap, IMX296_SHS1, ctrl->val, &ret);
            break;

        case V4L2_CID_ANALOGUE_GAIN:
            cci_write(sensor->regmap, IMX296_GAIN, ctrl->val, &ret);
            break;

        case V4L2_CID_VBLANK:
            cci_write(sensor->regmap, IMX296_VMAX, fmt->height + ctrl->valm &ret);
            break;

/* TODO
        case V4L2_CID_TEST_PATTERN:
            break;

        
        // Need to figure out format reneg for hflip/vflip because setting these will change the bayer pattern which means the format code should be adjusted
        case V4L2_CID_HFLIP:
        case V4L2_CID_VFLIP:

            u32 reg;

            reg =

            cci_write(sensor->regmap, IMX296_CTRL0E, reg, &ret);
            break;
*/

        default:
            ret = -EINVAL;
            break;

    }

    return ret;

}

static int imx296_ctrl_init(struct imx296* sensor);
{

    /* init v4l2_ctrl_handler */
    v4l2_ctrl_handler_init(&sensor->ctrl_handler, 1);

    /* set up controls 
        - exposure
        - gain
        - hblank/vblank
        - hfip/vflip
        - pixel rate
        - test pattern
    
    */

    /* exposure */
    v4l2_ctrl_new_std(&sensor->ctrl_handler, &imx296_ctrl_ops, V4L2_CID_EXPOSURE, 1, MAX_20_BITS, 1, EXPOSURE_DEF_VALUE);

    /* gain */
    v4l2_ctrl_new_std(&sensor->ctrl_handler, &imx296_ctrl_ops, V4L2_CID_ANALOGUE_GAIN, IMX296_GAIN_MIN, IMX296_GAIN_MAX, 1, IMX296_GAIN_MIN);

    /* vblank - unit is # of lines */
    v4l2_ctrl_new_std(&sensor->ctrl_handler, &imx296_ctrl_ops, V4L2_CID_VBLANK, 30, MAX_20_BITS - IMX296_MAX_HEIGHT, 1, 30);

    /* hblank - unit is number of pixels, the hblank cannot be changed by user */
    unsigned int hblank =  IMX296_HMAX_FIXED_VAL * (IMX296_PIXEL_RATE / IMX296_INCK_FREQ) - IMX296_MAX_WIDTH;

    v4l2_ctrl_new_std(&sensor->ctrl_handler, &imx296_ctrl_ops, V4L2_CID_HBLANK, hblank, hblank, 1, hblank);

    /* pixel rate - cannot be changed by user (will set to nominal 1118 Mbps ) */
    v4l2_ctrl_new_std(&sensor->ctrl_handler, &imx296_ctrl_ops, V4L2_CID_PIXEL_RATE, IMX296_PIXEL_RATE, IMX296_PIXEL_RATE, 1, IMX296_PIXEL_RATE);

    /* test pattern */
    v4l2_ctrl_new_std_menu_items(&sensor->ctrl_handler, &imx296_ctrl_ops, V4L2_CID_TEST_PATTERN, ARRAY_SIZE(imx296_test_pattern_menu) - 1, 0, 0, imx296_test_pattern_menu);

    /* hflip + vflip */
    sensor->hflip = v4l2_ctrl_new_std(&sensor->ctrl_handler, &imx296_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
    
    if (sensor->hflip)
    {
        /* set this to tell other functions the Bayer Format will change from BGGR to GBRG*/
        sensor->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

    }

    v4l2_ctrl_new_std(&sensor->ctrl_handler, &imx296_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

    if (sensor->vflip)
    {
        /* set this to tell other functions the Bayer Format will change from BGGR to GBRG*/
        sensor->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

    }


    if (sensor->ctrl_handler.error)
    {
        dev_err(sensor->sd->v4l2_dev->dev, "Failed to set up v4l2 controls, err: %d\n", sensor->ctrl_handler.error);
        v4l2_ctrl_handler_free(sensor->ctrl_handler);
        return sensor->ctrl_handler.error;
    }

    sensor->sd.ctrl_handler = &sensor->ctrl_handler;

    return 0;

}   

static int imx296_stream_on(struct imx296* sensor)
{
    int ret = 0;
    
    /* to start streaming - 1. set standby reg 2. set master mode reg */
    cci_write(sensor->regmap, IMX296_CTRL00, 0, &ret);
    usleep_range(2000, 5000);
    cci_write(sensor->regmap, IMX296_CTRL0A, 0, &ret);

    return ret;
}

static int imx296_stream_off(struct imx296* sensor)
{
    int ret = 0;
    
    /* to start streaming - 1. set standby reg 2. set master mode reg */
    cci_write(sensor->regmap, IMX296_CTRL00, IMX296_CTRL00_STANDBY, &ret);
    usleep_range(2000, 5000);
    cci_write(sensor->regmap, IMX296_CTRL0A, IMX296_CTRL0A_XMSTA, &ret);

    return ret;
}

static int imx296_setup(struct imx296* sensor, v4l2_subdev_state* state)
{
    const struct v4l2_mbus_framefmt *fmt;
    const struct v4l2_rect* crop;

    /* get crop and format settings from state */
    fmt = v4l2_subdev_state_get_format(state, 0);
    crop = v4l2_subdev_state_get_crop(state, 0);

    cci_multi_reg_write(sensor->regmap, &imx296_init_table, ARRAY_SIZE(imx296_init_table), &ret);

    /* set roi registers based on crop */
    if (crop->height != IMX296_MAX_HEIGHT || crop->width != IMX296_MAX_WIDTH)
    {
        /* enable vertical and horizontal roi area */
        cci_reg_write(sensor->regmap, IMX296_FID0_ROI, IMX296_FID0_ROIH1ON | IMX296_FID0_ROIV1ON, &ret);
        cci_reg_write(sensor->regmap, IMX296_FID0_ROIPH1, crop->top, &ret);
        cci_reg_write(sensor->regmap, IMX296_FID0_ROIPV1, crop->left, &ret);
        cci_reg_write(sensor->regmap, IMX296_FID0_ROIWH1, crop->width, &ret);
        cci_reg_write(sensor->regmap, IMX296_FID0_ROIWV1, crop->height, &ret);

    }
    else 
    {
        /* disable ROI */
        cci_reg_write(sensor->regmap, IMX296_FID0_ROI, 0, &ret);
    }

    /* perform horizontal and vertical binning if crop and format dimensions are not equal */
    cci_reg_write(sensor->regmap, IMX296_CTRL0D, (crop->width != fmt->width ? IMX296_CTRL0D_HADD_ON_BINNING : 0) | 
                                                    (crop->height != fmt->height ? IMX296_CTRL0D_WINMODE_FD_BINNING : 0) , &ret);

    /* set HMAX and VMAX */
    cci_reg_write(sensor->regmap, IMX296_HMAX, 1088U, &ret);
    cci_reg_write(sensor>regmap, IMX296_VMAX, fmt->height + sensor->vblank->cur.val, &ret);

    /* set INCK registers */
    for (unsigned int i = 0; i < ARRAY_SIZE(clk_params->inksel); i++)
    {
        cci_reg_write(sensor->regmap, IMX296_INCKSEL(i), clk_params->inksel[i], &ret);
    }

	imx296_write(sensor, IMX296_GTTABLENUM, 0xc5, &ret);
	imx296_write(sensor, IMX296_CTRL418C, sensor->clk_params->ctrl418c,
		     &ret);

    /* set gaindly and blacklevel */
    cci_reg_write(sensor->regmap, IMX296_BLKLEVEL, 0x3c, &ret);
    cci_reg_write(sensor->regmap, IMX296_GAINDLY, IMX296_GAINDLY_NONE, &ret);

    return ret;
}

/* V4L2 Subdev Operations */
static int imx296_set_stream()
{

} 


/* v4l2_subdev_pad_ops */
static int imx296_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_state* state, struct v4l2_subdev_mbus_code_enum *code)
{
    struct imx296* sensor = to_imx296(sd);

    /* check if the pad is valid ( imx296 only supports single pad and format )*/
    if (code->pad > 0 || code->index > 0)
        return -EINVAL;


    /* set format to code member based on if sensor is monochrome or coloured */    
    code->code = sensor->mono ? MEDIA_BUS_FMT_Y10_1X10 : MEDIA_BUS_FMT_SBGGR10_1X10;


    return 0;

}

static int imx296_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_state* state, struct v4l2_subdev_frame_size_enum *fse)
{
    /* two resolutions supported by imx296 

       Full Res: 1440 x 1080
       Bin Mode: 720 x 540

    */

    const struct v4l2_mbus_framefmt *format;

    format = v4l2_subdev_state_get_format(state, fse->pad);

    if (fse->index >= 2 || fse->code != format->code)
        return -EINVAL;

    /* set fse frame size */
    fse->min_width = (IMX296_MAX_WIDTH) / (fse->index +  1);
    fse->max_width = fse->min_width;

    fse->min_height = (IMX296_MAX_HEIGHT) / (fse->index + 1);
    fse->max_height = fse->min_height;

    return 0;

}

static int imx296_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state* state, struct v4l2_subdev_format *format)
{

    struct imx296 *sensor = to_imx296(sd);

    struct v4l2_mbus_framefmt *fmt = v4l2_subdev_state_get_format(state, format->pad);
    struct v4l2_rect *crop = v4l2_subdev_state_get_crop(state, format->pad);


    /* need to implement: crop resolution */

    /* if crop is disabled, binning can be setup */
    if  (crop->height == IMX296_MAX_HEIGHT && crop->width == IMX296_MAX_WIDTH)
    {

        unsigned int width;
        unsigned int height;        
        unsigned int hratio;       
        unsigned int vratio;

        width = clamp_t(unsigned int, format->format.width, crop->width/2, crop->width);
        height = clamp_t(unsigned int, format->format.height, crop->height/2, crop->height);

        hratio = DIV_ROUND_CLOSEST(crop->width, width);
        vratio = DIV_ROUND_CLOSEST(crop->height, height);

        fmt->width = crop->width / hratio;
        fmt->height = crop->height / vratio;
    }
    else 
    {
        fmt->width = crop->width;
        fmt->height = crop->height; 
    }


    /* set state format settings */
    fmt->code = sensor->mono ? MEDIA_BUS_FMT_Y10_1X10 : MEDIA_BUS_FMT_SBGGR10_1X10;
    fmt->field = V4L2_FIELD_NONE;
    fmt->colorspace = V4L2_COLORSPACE_RAW;
    fmt->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
    fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
    fmt->xfer_func = V4L2_XFER_FUNC_NONE;


    /* pass fmt back to caller argument */
    format->format = *fmt;


}


static int imx296_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state* state, struct v4l2_subdev_format *format)
{

    struct v4l2_mbus_framefmt *fmt = vl42_subdev_state_get_format(state, format->pad);


    if (!fmt)
        return -EINVAL;

    format->format = *fmt;

    return 0;

}

static int imx296_get_selection(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_selection *sel)
{
    switch (sel->target)
    {
        case V4L2_SEL_TGT_CROP:
            sel->r = v4l2_subdev_state_get_crop(state, sel->pad);
            return 0;
        
        case V4L2_SEL_TGT_CROP_BOUNDS:
        case V4L2_SEL_TGT_CROP_DEFAULT:
            sel->r.left = 0;
            sel->r.top = 0;
     		sel->r.width = IMX296_PIXEL_ARRAY_WIDTH;
		    sel->r.height = IMX296_PIXEL_ARRAY_HEIGHT;
		    return 0;
	default:
		return -EINVAL;       
        
    }

}


static int imx296_set_selection(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_selection *sel);
{
    struct v4l2_mbus_framefmt *format;
    struct v4l2_rect *crop;
    struct v4l2_rect rect;

    if (sel->target != V4L2_SEL_TGT_CROP)
        return -EINVAL;


    /* set crop dimensions for rect */
    /* the top-left pixel can't go past the point where crop width becomes less than IMX296_FID0_ROIWH1_MIN */
    rect.left = clamp(ALIGN(sel->r.left, 4), 0, IMX296_MAX_WIDTH - IMX296_FID0_ROIWH1_MIN);

    rect.top = clamp(ALIGN(sel->r.top, 4), 0, IMX296_MAX_HEIGHT - IMX296_FID0_ROIWV1_MIN);
    
    rect.width = clamp(ALIGN(sel->r.width, 4), IMX296_FID0_ROIWH1_MIN, IMX296_MAX_WIDTH);

    rect.height = clamp(ALIGN(sel->r.height, 4), IMX296_FID0_ROIWV1_MIN, IMX296_MAX_HEIGHT);

    /* clamp width and height further to prevent crop from extending past bottom right of pixel array */
    rect.width = min_t(unsigned int, rect.width, IMX296_MAX_WIDTH - rect.left);
    rect.height = min_t(unsigned int, rect.height, IMX296_MAX_HEIGHT - rect.top);


    crop =  v4l2_subdev_get_crop(state, sel->pad);


    /* check crop dimensions match with rect, if not adjust format dimensions */
    if (crop->width != rect.width || crop->height != rect.height)
    {
        format = v4l2_subdev_get_format(state, sel->pad);
        format->width = rect.width;
        format->height = rect.height;
    }

    /* assign rect dimensions to state crop (used by set_fmt)*/
    *crop = rect;
    sel->r = rect;


    return 0;

}

static int imx296_init_state(struct v4l2_subdev* sd, struct v4l2_subdev_state state)
{
    /* set format and selection */
    struct v4l2_subdev_selection sel = {
        .target = V4L2_SEL_TGT_CROP,
        .r.width = IMX296_MAX_WIDTH,
        .r.height = IMX296_MAX_HEIGHT,
    };

    struct v4l2_subdev_format fmt = {
        .format = 
        {
            .width = IMX296_MAX_WIDTH,
            .height = IMX296_MAX_HEIGHT,
        },
    };

    imx296_set_selection(sd, state, &sel);
    imx296_set_format(sd, state, &fmt);

    return 0;
}


/* Power Management */

// need to add correct delays for power_on
static int imx296_power_on(struct imx296* sensor)
{

    /** Refer to datasheet pg 74 for power-on sequence
     * 
     * 1. Bring up power rails DVdd -> OVdd -> AVdd
     * 2. Set reset pin XCLR to reset register values
     * 3. Start input clock INCK
     * 
     *  
     */

    // brings up all three power rails
    int return = regulator_bulk_enable(ARRAY_SIZE(sensor->supplies), sensor->supplies);

    if (ret < 0)
        return ret;

    // set reset gpio pin
    ret = gpiod_direction_output(sensor->reset, 0);

    if (ret < 0)
        goto err_power;
    
    // start input clock INCK
    ret = clk_prepare_enable(sensor->clk);

    if (ret < 0)
        goto err_reset;

err_power:
    regulator_bulk_disable(ARRAY_SIZE(sensor->supplies), sensor->supplies);
err_reset:
    gpiod_direction_output(sensor->reset, 1);


    return ret;

}


static void imx296_power_off(struct imx296* sensor)
{
    /* reverse the steps done in imx296_power_off */
    clk_disable_unprepare(sensor->clk);
    gpiod_direction_output(sensor->reset, 1);
    regulator_bulk_disable(ARRAY_SIZE(sensor->supplies), sensor->supplies);

}


/******** init's **********/

/* all v4l2 subdev related init's belong in this function */
static int imx296_subdev_init(struct imx296* sensor)
{

    struct i2c_client* client = to_i2c_client(sensor->dev);
    v4l2_i2c_subdev_init(&sensor->sd, client, &imx296_subdev_ops);
    sensor->sd.internal_ops = &imx296_internal_ops;

    /* v4l2 ctrls init */
    ret = imx296_ctrls_init(sensor);
    if (ret < 0)
        return ret;

    /* create /dev/ node and enable v4l2 events */
    sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;

    /* media pad init */
    sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
    sensor->sd.entity.function = MEDIA_ENT_FT_CAM_SENSOR;
    ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);

    /* need to call v4l2_subdev_init_finalize */
}


static int imx296_probe(struct i2c_client *client)
{


}


static void imx296_remove(struct i2c_client *client)
{

    /* need to call v4l2_device_unregister_subdev() */

}


static const struct of_device_id ixm296_of_match[] = {
    {.compatible = "sony,imx296"},
};

MODULE_DEVICE_TABLE(of, imx296_of_match)

static struct i2c_driver imx296_i2c_driver = 
{
    .driver = {
        .name               = "imx296",
        .of_match_table     = imx296_of_match,
        .pm                 = ,
    },
    .probe      = imx296_probe,
    .remove     = imx296_remove,
};

module_i2c_driver(imx296_i2c_driver);


MODULE_AUTHOR("Branavan Keethabaskaran <brana.keeth@gmail.com>")
MODULE_DESCRIPTION("SONY IMX296 Camera Driver")
MODULE_LICENSE("GPL")